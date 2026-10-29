#pragma once

#include <QVector>
#include <QtGlobal>

#include <cmath>

/**
 * @brief Linear audio resampling (REQ-SW-PL-076).
 *
 * Port of the private `AudioPcm::resampleMono()`
 * (`plugins/ai_studio_plugin/core/shared/AudioPcm.cpp:40-63`) so the public
 * audio-output path does not depend on the AI plugin. Linear interpolation is
 * deliberate: the input is a microphone/TTS stream (typically 44.1/48 kHz →
 * 48 kHz device format) and the audio device is not the quality bottleneck —
 * the sink only has to avoid the "wrong pitch" artefact of a naive
 * drop/duplicate.
 *
 * Both functions are pure: no state, no allocation beyond the result, safe to
 * call from the audio path of a single node.
 */
namespace AudioResample {

/**
 * @brief Resample one mono channel.
 *
 * Linear interpolation between neighbouring input samples; the last output
 * sample never reads past the end of the input. Equal rates are a no-op
 * (the input is returned unchanged — the fast path that keeps 48 kHz device
 * playback copy-free).
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

    const double ratio = outRate / inRate;                       // < 1 → decimate
    const int outCount = int(std::llround(double(in.size()) * ratio));
    if (outCount <= 0)
        return {};

    QVector<float> out(outCount);
    const int lastIndex = in.size() - 1;
    for (int i = 0; i < outCount; ++i) {
        const double srcPos = double(i) / ratio;                 // position in the input
        const int i0 = int(std::floor(srcPos));
        if (i0 >= in.size()) {
            out[i] = in.constLast();
            continue;
        }
        const int i1 = qMin(i0 + 1, lastIndex);
        const double frac = srcPos - double(i0);
        out[i] = float(double(in.at(i0)) * (1.0 - frac) + double(in.at(i1)) * frac);
    }
    return out;
}

/**
 * @brief Resample every channel of a de-interleaved buffer.
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

    QVector<QVector<float>> out;
    out.reserve(channels.size());
    for (const QVector<float> &channel : channels) {
        const QVector<float> resampled = resampleMono(channel, inRate, outRate);
        if (resampled.isEmpty())
            return {};                                          // never emit a partial block
        out.append(resampled);
    }
    return out;
}

} // namespace AudioResample
