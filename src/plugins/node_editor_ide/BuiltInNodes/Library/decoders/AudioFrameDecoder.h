#ifndef AUDIOFRAMEDECODER_H
#define AUDIOFRAMEDECODER_H

#include "NodeDataTypes/SampledStreamDescriptor.h"

#include <QtGlobal>
#include <QtMultimedia/QAudioFormat>

/**
 * @brief Decode a single PCM sample from raw bytes and return a normalized value in [-1, 1].
 *
 * The decoder is configured once per audio format and used in the hot path without
 * format branching. Two configuration entry points exist (REQ-SW-PL-022):
 *  - configure(const QAudioFormat&) — legacy QtMultimedia path used by the
 *    QDevIO mic display (XYSeriesIODevice);
 *  - configure(SampleType, int, SampleEndian) — QtCore-only overload used by
 *    the unified SampledData world; it shares the same normalization convention
 *    (signed/unsigned divided by (2^(bits-1) − 1), clamped to [-1, 1]; floats
 *    clamped) as SampledDecoder in SampledData.h.
 */
class AudioFrameDecoder
{
public:
    AudioFrameDecoder();

    bool configure(const QAudioFormat &format);

    /// QtCore-only overload (REQ-SW-PL-022 AC 3): configure from the unified
    /// SampleType descriptor. `sampleBits` is the storage bit depth (8/16/24/32/64).
    bool configure(SampleType sampleType, int sampleBits, SampleEndian endian);

    int bytesPerSample() const { return m_bytesPerSample; }
    int channels() const { return m_channels; }
    int frameBytes() const { return m_bytesPerSample * m_channels; }

    qreal decodeNormalizedSample(const char *samplePtr) const;
    bool isValid() const { return m_decoder != nullptr; }

private:
    using DecoderFn = qreal (*)(const char *);

    DecoderFn m_decoder = nullptr;
    int m_bytesPerSample = 2;
    int m_channels = 1;
};

#endif // AUDIOFRAMEDECODER_H