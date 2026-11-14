#ifndef AUDIODSPSUPPORT_H
#define AUDIODSPSUPPORT_H

// SPDX-License-Identifier: MIT

// Shared glue for the rate-domain nodes (REQ-SW-PL-092):
// `BlockResizerNode`, `SampleDecimatorNode`, `SampleResamplerNode`.
//
// Two things every one of them needs and none of them should invent again:
//
//   * **"did the stream change under me?"** — a rate or channel-count change in
//     the middle of a stream is not a new block, it is a broken clock; the
//     node reports it instead of guessing whether the numbers still mean the
//     same thing;
//   * **how the output block is built** — always normalized FLOAT32, little
//     endian, one channel per input channel, with the sample rate the rate
//     conversion implies. The DSP engines are mono, so the node loops over
//     channels and interleaves.
//
// Header-only and `internal` in spirit: it is included by the three node
// implementation files (and by their headers where a member needs the
// fingerprint), because it is not part of any node's public API.

#include "NodeDataTypes/SampledData.h"

#include <QByteArray>
#include <QVector>
#include <QtEndian>

#include <memory>

namespace AudioDspSupport {

/// What a rate-domain node remembers about the stream it is attached to.
struct StreamFingerprint {
    int channels = 0;
    int bytesPerFrame = 0;
    double sampleRate = 0.0;
    bool set = false;

    /// Remember the first block's descriptor.
    void adopt(const SampledStreamDescriptor& descriptor)
    {
        channels = descriptor.totalChannels();
        bytesPerFrame = descriptor.bytesPerFrame();
        sampleRate = descriptor.sampleRate;
        set = true;
    }

    /**
     * @brief Why a later block does not belong to this stream.
     *
     * @param descriptor Descriptor of the block that just arrived.
     * @return An empty string when it matches, otherwise the reason — naming
     *         the value, because "inconsistent input" is not actionable.
     */
    QString mismatch(const SampledStreamDescriptor& descriptor) const
    {
        if (!set)
            return QString();
        if (descriptor.totalChannels() != channels) {
            return QStringLiteral("каналите се смениха по средата на потока: %1 → %2")
                .arg(channels)
                .arg(descriptor.totalChannels());
        }
        if (descriptor.bytesPerFrame() != bytesPerFrame) {
            return QStringLiteral("размерът на кадъра се смени по средата на потока: %1 → %2 байта")
                .arg(bytesPerFrame)
                .arg(descriptor.bytesPerFrame());
        }
        if (qAbs(descriptor.sampleRate - sampleRate) > 1e-9) {
            return QStringLiteral("честотата се смени по средата на потока: %1 → %2 Hz")
                .arg(sampleRate, 0, 'g', 10)
                .arg(descriptor.sampleRate, 0, 'g', 10);
        }
        return QString();
    }
};

/// Decode every channel of @p data to normalized float samples.
inline QVector<QVector<float>> decodeChannels(const SampledData& data)
{
    QVector<QVector<float>> channels;
    data.decodeToNormalizedF32(channels);
    return channels;
}

/// Number of frames in the shortest channel (0 when nothing decoded).
inline int frameCount(const QVector<QVector<float>>& channels)
{
    int frames = channels.isEmpty() ? 0 : channels.constFirst().size();
    for (const QVector<float>& channel : channels)
        frames = qMin(frames, channel.size());
    return frames;
}

/**
 * @brief Build a normalized-FLOAT32 block from per-channel samples.
 *
 * The descriptor of the input is reused for everything that is not the rate —
 * channel names, domain, source name, endianness policy — and the sample type
 * becomes FLOAT32 because the DSP engines speak floats. `unit` becomes
 * "normalized" and the amplitude mapping is cleared: the values are no longer
 * raw device counts, so a scale left behind would label them wrongly.
 *
 * @param input      The block the samples came from.
 * @param sampleRate Output rate (Hz).
 * @param firstIndex Index of the first output frame, or −1 when unknown.
 * @param channels   Per-channel samples, all the same length.
 * @return The block, or nullptr when there is nothing to publish.
 */
inline std::shared_ptr<SampledData> makeFloat32Block(const SampledData& input,
                                                     double sampleRate,
                                                     qint64 firstIndex,
                                                     const QVector<QVector<float>>& channels)
{
    const int channelsCount = channels.size();
    const int frames = frameCount(channels);
    if (channelsCount <= 0 || frames <= 0 || sampleRate <= 0.0)
        return nullptr;

    SampledStreamDescriptor descriptor = input.descriptor();
    descriptor.sampleRate = sampleRate;
    descriptor.unit = QStringLiteral("normalized");
    descriptor.amplitudeScale = 1.0;
    descriptor.amplitudeOffset = 0.0;
    for (StreamChannelDescriptor& channel : descriptor.channels)
        channel.sampleType = SampleType::FLOAT32;

    QByteArray buffer;
    buffer.resize(frames * channelsCount * int(sizeof(float)));
    char* out = buffer.data();
    for (int frame = 0; frame < frames; ++frame) {
        for (int channel = 0; channel < channelsCount; ++channel) {
            const float value = channels.at(channel).at(frame);
            qToLittleEndian<float>(value, out);
            out += int(sizeof(float));
        }
    }

    return std::make_shared<SampledData>(std::move(buffer), descriptor, firstIndex);
}

} // namespace AudioDspSupport

#endif // AUDIODSPSUPPORT_H
