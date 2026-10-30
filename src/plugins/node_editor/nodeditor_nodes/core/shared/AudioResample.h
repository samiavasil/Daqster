#pragma once

#include <QVector>
#include <QtGlobal>

#include <algorithm>
#include <cmath>

/**
 * @brief Linear audio resampling with carried state (REQ-SW-PL-076, AC9).
 *
 * Port of the private `AudioPcm::resampleMono()`
 * (`plugins/ai_studio_plugin/core/shared/AudioPcm.cpp:40-63`) so the public
 * audio-output path does not depend on the AI plugin. Linear interpolation is
 * deliberate: the input is a microphone/TTS stream (typically 44.1/48 kHz →
 * 48 kHz device format) and the audio device is not the quality bottleneck —
 * the sink only has to avoid the "wrong pitch" artefact of a naive
 * drop/duplicate.
 *
 * `resampleMono()`/`resampleChannels()` are stateless one-block wrappers (tests,
 * backward compatibility): they run a fresh `RateConverter` over a single
 * block, i.e. "this block is the whole stream".
 */
namespace AudioResample {

/**
 * @brief Streaming linear resampler: one instance per playback sink.
 *
 * Keeps the three pieces of state a streaming resampler needs, PER CHANNEL:
 *   - `srcPos` — the fractional phase of the next output sample, measured
 *     against the current block;
 *   - `lastIn` — the previous block's last input sample: the left neighbour of
 *     an interpolation that crosses the block boundary;
 *   - `accum` — how many output samples were already emitted, so the block
 *     length is never rounded independently (see `resampleChannel()`).
 *
 * Not thread-safe: one instance belongs to one audio path (the
 * `AudioPlaybackEngine` owns it and calls it from `push()`).
 *
 * `reset()` whenever audio that was already converted is discarded — a
 * (re)created sink, a cleared queue, a changed device format — otherwise the
 * carried phase belongs to audio nobody will hear and the next block is offset
 * by up to one input sample.
 */
class RateConverter
{
public:
    /// Per-channel state: the three numbers that make a block splice seamless.
    struct ChannelState {
        double srcPos = 0.0;  ///< phase of the next output sample (block-relative)
        float lastIn = 0.0f;  ///< last input sample of the previous block
        double accum = 0.0;   ///< output samples already emitted (integer-valued)
    };

    RateConverter() = default;

    /**
     * @brief Configure the rates and reset the state.
     *
     * @param inRate  Input sample rate in Hz
     * @param outRate Output sample rate in Hz
     */
    RateConverter(double inRate, double outRate) { configure(inRate, outRate); }

    /**
     * @brief Set the rates; a rate change starts a new stream (state is reset).
     *
     * Equal rates (`qFuzzyCompare`) are the pass-through fast path: the input
     * is returned untouched and NO state is built or advanced.
     *
     * @param inRate  Input sample rate in Hz (> 0)
     * @param outRate Output sample rate in Hz (> 0)
     */
    void configure(double inRate, double outRate)
    {
        if (qFuzzyCompare(inRate, m_inRate) && qFuzzyCompare(outRate, m_outRate))
            return;
        m_inRate = inRate;
        m_outRate = outRate;
        reset();
    }

    /// Drop all state: the next block starts a new stream at phase 0.
    void reset()
    {
        m_channels.clear();
        m_inputCount = 0;
        m_produced = 0;
    }

    double inRate() const { return m_inRate; }
    double outRate() const { return m_outRate; }

    /// True when no resampling happens at all (equal rates, or unusable rates).
    bool isPassThrough() const
    {
        return m_inRate <= 0.0 || m_outRate <= 0.0 || qFuzzyCompare(m_inRate, m_outRate);
    }

    /// Output samples emitted since the last `reset()`.
    qint64 produced() const { return m_produced; }

    /// State kept per channel (empty before the first block / in pass-through).
    const QVector<ChannelState> &channelStates() const { return m_channels; }

    /**
     * @brief Resample one mono block, continuing where the previous block ended.
     *
     * @param in Input samples
     * @return Samples at `outRate()`; empty for unusable input
     */
    QVector<float> process(const QVector<float> &in)
    {
        if (in.isEmpty() || m_inRate <= 0.0 || m_outRate <= 0.0)
            return {};
        if (qFuzzyCompare(m_inRate, m_outRate))
            return in;                   // pass-through: no state, values untouched
        if (!ensureChannels(1))
            return {};

        const int outCount = int(outputCountFor(in.size()));
        m_inputCount += in.size();
        m_produced += outCount;

        QVector<float> out;
        resampleChannel(m_channels[0], in, outCount, out);
        return out;
    }

    /**
     * @brief Resample every channel of a de-interleaved block.
     *
     * All channels are fed the same block length; the output length is computed
     * once and applied to all of them, so the channels stay sample-aligned.
     *
     * @param in  Per-channel samples
     * @param out Receives per-channel samples at `outRate()`
     * @return false when the input is unusable; a pass-through returns it as-is
     */
    bool processChannels(const QVector<QVector<float>> &in, QVector<QVector<float>> &out)
    {
        out.clear();
        if (in.isEmpty() || in.constFirst().isEmpty())
            return false;
        if (m_inRate <= 0.0 || m_outRate <= 0.0)
            return false;
        if (qFuzzyCompare(m_inRate, m_outRate)) {
            out = in;                    // pass-through: no state, values untouched
            return true;
        }
        if (!ensureChannels(in.size()))
            return false;

        const int outCount = int(outputCountFor(in.constFirst().size()));
        m_inputCount += in.constFirst().size();
        m_produced += outCount;

        out.resize(in.size());
        for (int channel = 0; channel < in.size(); ++channel)
            resampleChannel(m_channels[channel], in.at(channel), outCount, out[channel]);
        return true;
    }

private:
    /**
     * @brief Output samples this block must produce.
     *
     * Derived from the TOTAL input consumed since the last reset, never from
     * this block alone: `round(size * ratio)` per block rounds up to half a
     * sample independently every time, which at 44100 → 48000 adds **0.32
     * samples per block** (108 extra samples over 300 blocks of 256 — audible
     * as pitch/timing drift on top of the clicks).
     *
     * The rule is "emit every output whose source position lies inside the
     * samples that have arrived": position `k / ratio` must be ≤ `totalIn - 1`.
     * Written as a difference of counts it is the accumulation rule
     * `out = floor(prev + N*ratio) - prev` with `prev` being the number of
     * samples already emitted — except that the phase reference sits ONE input
     * sample back, so the last output of a block never needs a sample that has
     * not arrived yet (otherwise it would have to be clamped to itself, which is
     * exactly the artefact this class removes).
     *
     * @param inputCount Samples in the block about to be processed (> 0)
     * @return Samples to emit for that block (≥ 0)
     */
    qint64 outputCountFor(int inputCount) const
    {
        if (inputCount <= 0)
            return 0;
        const double ratio = m_outRate / m_inRate;
        const double lastPosition = double(m_inputCount + inputCount - 1) * ratio;
        const qint64 target = qint64(std::floor(lastPosition)) + 1;
        return qMax<qint64>(0, target - m_produced);
    }

    /// Build the per-channel state for @p count channels.
    ///
    /// A different channel layout is a different stream: phases from the old one
    /// are meaningless, so it is restarted rather than extended.
    ///
    /// @return false when nothing can be resampled (pass-through, unusable rates)
    bool ensureChannels(int count)
    {
        if (isPassThrough())
            return false;
        if (m_channels.size() != count) {
            reset();
            m_channels.resize(count);
        }
        return true;
    }

    /**
     * @brief Resample one channel of @p in into @p out, advancing @p state.
     *
     * `state.srcPos` starts in `(-1, 0)`: the next output sample of a continuing
     * stream lies between the previous block's last sample (kept in
     * `state.lastIn`) and this block's first. Interpolation therefore crosses
     * the boundary instead of restarting at 0.
     *
     * @param state    Per-channel state (updated in place)
     * @param in       Input samples of this block
     * @param outCount Output samples to produce (from outputCountFor())
     * @param out      Receives the resampled block
     */
    void resampleChannel(ChannelState &state, const QVector<float> &in, int outCount,
                         QVector<float> &out)
    {
        const int frames = in.size();
        const double step = m_inRate / m_outRate;   // input samples per output sample

        out.resize(qMax(outCount, 0));
        for (int i = 0; i < out.size(); ++i) {
            const double position = state.srcPos;
            const double base = std::floor(position);
            const int left = int(base);              // -1 == the previous block's tail
            const double frac = position - base;
            const float a = (left < 0) ? state.lastIn
                                       : in.at(std::min(left, frames - 1));
            const int right = left + 1;
            const float b = (right < 0) ? state.lastIn
                                        : in.at(std::min(right, frames - 1));
            out[i] = float(double(a) * (1.0 - frac) + double(b) * frac);
            state.srcPos = position + step;
        }

        // The tail of this block is the left neighbour of the next one, and the
        // phase is rebased into the next block's coordinates (its first sample
        // is index 0, the previous block's last is index -1).
        state.lastIn = in.constLast();
        state.srcPos -= double(frames);
        state.accum += double(outCount);
    }

    double m_inRate = 0.0;
    double m_outRate = 0.0;
    QVector<ChannelState> m_channels;
    /// Input samples consumed since the last reset.
    qint64 m_inputCount = 0;
    /// Output samples emitted since the last reset (== every `ChannelState::accum`).
    qint64 m_produced = 0;
};

/**
 * @brief Resample one mono channel as a whole stream (stateless wrapper).
 *
 * Equal rates are a no-op (the input is returned unchanged — the fast path that
 * keeps 48 kHz device playback copy-free).
 *
 * @param in      Input samples
 * @param inRate  Input sample rate in Hz (> 0)
 * @param outRate Output sample rate in Hz (> 0)
 * @return Samples at @p outRate; empty for an empty input or a non-positive rate
 */
inline QVector<float> resampleMono(const QVector<float> &in, double inRate, double outRate)
{
    if (in.isEmpty() || inRate <= 0.0 || outRate <= 0.0)
        return {};
    if (qFuzzyCompare(inRate, outRate))
        return in;
    RateConverter converter(inRate, outRate);
    return converter.process(in);
}

/**
 * @brief Resample every channel of a de-interleaved buffer (stateless wrapper).
 *
 * @param channels Per-channel samples (all at @p inRate)
 * @param inRate   Input sample rate in Hz (> 0)
 * @param outRate  Output sample rate in Hz (> 0)
 * @return Per-channel samples at @p outRate; empty when the input is unusable
 */
inline QVector<QVector<float>> resampleChannels(const QVector<QVector<float>> &channels,
                                                double inRate,
                                                double outRate)
{
    if (channels.isEmpty())
        return {};

    RateConverter converter(inRate, outRate);
    QVector<QVector<float>> out;
    if (!converter.processChannels(channels, out))
        return {};
    return out;
}

} // namespace AudioResample
