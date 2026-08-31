#pragma once

#include "SampledStreamDescriptor.h"

#include <QByteArray>
#include <QVector>
#include <QtGlobal>

#include <QtNodes/NodeData>

#include <cmath>
#include <cstdint>
#include <cstring>

namespace SampledDecoder {

/**
 * @brief Unified sample normalization convention (REQ-SW-PL-022 AC 3).
 *
 * Signed integer samples are divided by (2^(bits-1) − 1), i.e. the maximum
 * positive value (32767 for int16, 8388607 for int24, ...), and the result is
 * clamped to [-1, 1]. Unsigned samples are first centered (subtract the
 * midpoint 2^(bits-1)) and then divided by the same (2^(bits-1) − 1) factor.
 * Floating point samples are only clamped to [-1, 1].
 *
 * This is the AudioFrameDecoder convention (32767 + clamp). It deliberately
 * fixes the historical inconsistency where GenericNumericTypes.cpp divided by
 * 2^(bits-1) (32768.0) without clamping while AudioFrameDecoder.cpp used
 * (2^(bits-1) − 1) with clamping.
 */
inline qreal clampUnit(qreal x)
{
    if (x > 1.0) return 1.0;
    if (x < -1.0) return -1.0;
    return x;
}

// --- Signed integer decoders ------------------------------------------------

inline qreal decodeS8(const char *p)
{
    return clampUnit(qreal(*reinterpret_cast<const qint8 *>(p)) / qreal(127.0));
}

inline qreal decodeS16(const char *p, SampleEndian endian)
{
    const quint8 b0 = static_cast<quint8>(p[endian == SampleEndian::BigEndian ? 1 : 0]);
    const quint8 b1 = static_cast<quint8>(p[endian == SampleEndian::BigEndian ? 0 : 1]);
    const qint16 v = static_cast<qint16>(b0 | (static_cast<quint16>(b1) << 8));
    return clampUnit(qreal(v) / qreal(32767.0));
}

inline qreal decodeS24(const char *p, SampleEndian endian)
{
    const int b0 = static_cast<quint8>(p[endian == SampleEndian::BigEndian ? 2 : 0]);
    const int b1 = static_cast<quint8>(p[endian == SampleEndian::BigEndian ? 1 : 1]);
    const int b2 = static_cast<quint8>(p[endian == SampleEndian::BigEndian ? 0 : 2]);
    const qint32 v = b0 | (b1 << 8) | (static_cast<qint8>(b2) << 16);
    return clampUnit(qreal(v) / qreal(8388607.0));
}

inline qreal decodeS32(const char *p, SampleEndian endian)
{
    quint32 bits = 0;
    for (int i = 0; i < 4; ++i) {
        const int src = endian == SampleEndian::BigEndian ? (3 - i) : i;
        bits |= static_cast<quint32>(static_cast<quint8>(p[src])) << (8 * i);
    }
    const qint32 v = static_cast<qint32>(bits);
    return clampUnit(qreal(v) / qreal(2147483647.0));
}

// --- Unsigned integer decoders ---------------------------------------------

inline qreal decodeU8(const char *p)
{
    const int v = static_cast<quint8>(p[0]);
    return clampUnit(qreal(v - 128) / qreal(127.0));
}

inline qreal decodeU16(const char *p, SampleEndian endian)
{
    const quint8 b0 = static_cast<quint8>(p[endian == SampleEndian::BigEndian ? 1 : 0]);
    const quint8 b1 = static_cast<quint8>(p[endian == SampleEndian::BigEndian ? 0 : 1]);
    const int v = static_cast<int>(b0 | (static_cast<quint16>(b1) << 8));
    return clampUnit(qreal(v - 32768) / qreal(32767.0));
}

inline qreal decodeU24(const char *p, SampleEndian endian)
{
    const int b0 = static_cast<quint8>(p[endian == SampleEndian::BigEndian ? 2 : 0]);
    const int b1 = static_cast<quint8>(p[endian == SampleEndian::BigEndian ? 1 : 1]);
    const int b2 = static_cast<quint8>(p[endian == SampleEndian::BigEndian ? 0 : 2]);
    const int v = b0 | (b1 << 8) | (b2 << 16);
    return clampUnit(qreal(v - 8388608) / qreal(8388607.0));
}

inline qreal decodeU32(const char *p, SampleEndian endian)
{
    quint32 bits = 0;
    for (int i = 0; i < 4; ++i) {
        const int src = endian == SampleEndian::BigEndian ? (3 - i) : i;
        bits |= static_cast<quint32>(static_cast<quint8>(p[src])) << (8 * i);
    }
    return clampUnit(qreal(static_cast<qint64>(bits) - 2147483648LL)
                     / qreal(2147483647.0));
}

// --- Float decoders ---------------------------------------------------------

inline qreal decodeF32(const char *p, SampleEndian endian)
{
    quint32 bits = 0;
    for (int i = 0; i < 4; ++i) {
        const int src = endian == SampleEndian::BigEndian ? (3 - i) : i;
        bits |= static_cast<quint32>(static_cast<quint8>(p[src])) << (8 * i);
    }
    float value = 0.0F;
    std::memcpy(&value, &bits, sizeof(value));
    if (value > 1.0F) value = 1.0F;
    if (value < -1.0F) value = -1.0F;
    return qreal(value);
}

inline qreal decodeF64(const char *p, SampleEndian endian)
{
    quint64 bits = 0;
    for (int i = 0; i < 8; ++i) {
        const int src = endian == SampleEndian::BigEndian ? (7 - i) : i;
        bits |= static_cast<quint64>(static_cast<quint8>(p[src])) << (8 * i);
    }
    double value = 0.0;
    std::memcpy(&value, &bits, sizeof(value));
    if (value > 1.0) value = 1.0;
    if (value < -1.0) value = -1.0;
    return value;
}

// --- Raw decoders (no normalization, no centering, no clamp) -----------------
//
// Physical-decode helpers for SampledData::decodeToPhysical() (REQ-SW-PL-025
// AC 1): the raw integer value is returned AS-IS (signed integers keep their
// sign, unsigned integers are NOT centered) and floats pass through unchanged —
// the caller applies `raw × amplitudeScale + amplitudeOffset`. Results may fall
// outside [-1, 1] (e.g. ±10 V DAQ input) — that is the point.

inline qreal rawS8(const char *p)
{
    return qreal(*reinterpret_cast<const qint8 *>(p));
}

inline qreal rawS16(const char *p, SampleEndian endian)
{
    const quint8 b0 = static_cast<quint8>(p[endian == SampleEndian::BigEndian ? 1 : 0]);
    const quint8 b1 = static_cast<quint8>(p[endian == SampleEndian::BigEndian ? 0 : 1]);
    return qreal(static_cast<qint16>(b0 | (static_cast<quint16>(b1) << 8)));
}

inline qreal rawS24(const char *p, SampleEndian endian)
{
    const int b0 = static_cast<quint8>(p[endian == SampleEndian::BigEndian ? 2 : 0]);
    const int b1 = static_cast<quint8>(p[endian == SampleEndian::BigEndian ? 1 : 1]);
    const int b2 = static_cast<quint8>(p[endian == SampleEndian::BigEndian ? 0 : 2]);
    const qint32 v = b0 | (b1 << 8) | (static_cast<qint8>(b2) << 16);
    return qreal(v);
}

inline qreal rawS32(const char *p, SampleEndian endian)
{
    quint32 bits = 0;
    for (int i = 0; i < 4; ++i) {
        const int src = endian == SampleEndian::BigEndian ? (3 - i) : i;
        bits |= static_cast<quint32>(static_cast<quint8>(p[src])) << (8 * i);
    }
    return qreal(static_cast<qint32>(bits));
}

inline qreal rawU8(const char *p)
{
    return qreal(static_cast<quint8>(p[0]));
}

inline qreal rawU16(const char *p, SampleEndian endian)
{
    const quint8 b0 = static_cast<quint8>(p[endian == SampleEndian::BigEndian ? 1 : 0]);
    const quint8 b1 = static_cast<quint8>(p[endian == SampleEndian::BigEndian ? 0 : 1]);
    return qreal(static_cast<int>(b0 | (static_cast<quint16>(b1) << 8)));
}

inline qreal rawU24(const char *p, SampleEndian endian)
{
    const int b0 = static_cast<quint8>(p[endian == SampleEndian::BigEndian ? 2 : 0]);
    const int b1 = static_cast<quint8>(p[endian == SampleEndian::BigEndian ? 1 : 1]);
    const int b2 = static_cast<quint8>(p[endian == SampleEndian::BigEndian ? 0 : 2]);
    return qreal(b0 | (b1 << 8) | (b2 << 16));
}

inline qreal rawU32(const char *p, SampleEndian endian)
{
    quint32 bits = 0;
    for (int i = 0; i < 4; ++i) {
        const int src = endian == SampleEndian::BigEndian ? (3 - i) : i;
        bits |= static_cast<quint32>(static_cast<quint8>(p[src])) << (8 * i);
    }
    return qreal(static_cast<qint64>(bits));
}

inline qreal rawF32(const char *p, SampleEndian endian)
{
    quint32 bits = 0;
    for (int i = 0; i < 4; ++i) {
        const int src = endian == SampleEndian::BigEndian ? (3 - i) : i;
        bits |= static_cast<quint32>(static_cast<quint8>(p[src])) << (8 * i);
    }
    float value = 0.0F;
    std::memcpy(&value, &bits, sizeof(value));
    return qreal(value);
}

inline qreal rawF64(const char *p, SampleEndian endian)
{
    quint64 bits = 0;
    for (int i = 0; i < 8; ++i) {
        const int src = endian == SampleEndian::BigEndian ? (7 - i) : i;
        bits |= static_cast<quint64>(static_cast<quint8>(p[src])) << (8 * i);
    }
    double value = 0.0;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

/**
 * @brief Decode one sample of `type` from `samplePtr` to its raw value.
 *
 * Physical counterpart of decodeNormalizedSample() (REQ-SW-PL-025 AC 1): no
 * division by (2^(bits-1) − 1), no unsigned centering, no clamp — the raw
 * integer / float value is returned and the caller applies
 * `raw × amplitudeScale + amplitudeOffset`.
 */
inline qreal decodeRawSample(const char *samplePtr, SampleType type,
                             SampleEndian endian)
{
    switch (type) {
    case SampleType::INT8:    return rawS8(samplePtr);
    case SampleType::UINT8:   return rawU8(samplePtr);
    case SampleType::INT16:   return rawS16(samplePtr, endian);
    case SampleType::UINT16:  return rawU16(samplePtr, endian);
    case SampleType::INT24:   return rawS24(samplePtr, endian);
    case SampleType::UINT24:  return rawU24(samplePtr, endian);
    case SampleType::INT32:   return rawS32(samplePtr, endian);
    case SampleType::UINT32:  return rawU32(samplePtr, endian);
    case SampleType::FLOAT32: return rawF32(samplePtr, endian);
    case SampleType::FLOAT64: return rawF64(samplePtr, endian);
    }
    return 0.0;
}

/**
 * @brief Decode one sample of `type` from `samplePtr` to [-1, 1].
 *
 * This is the single canonical decoder used by SampledData::decodeToNormalized()
 * and by AudioFrameDecoder's QtCore-only configure(SampleType, bits, endian)
 * overload (REQ-SW-PL-022 AC 3).
 */
inline qreal decodeNormalizedSample(const char *samplePtr, SampleType type,
                                    SampleEndian endian)
{
    switch (type) {
    case SampleType::INT8:    return decodeS8(samplePtr);
    case SampleType::UINT8:   return decodeU8(samplePtr);
    case SampleType::INT16:   return decodeS16(samplePtr, endian);
    case SampleType::UINT16:  return decodeU16(samplePtr, endian);
    case SampleType::INT24:   return decodeS24(samplePtr, endian);
    case SampleType::UINT24:  return decodeU24(samplePtr, endian);
    case SampleType::INT32:   return decodeS32(samplePtr, endian);
    case SampleType::UINT32:  return decodeU32(samplePtr, endian);
    case SampleType::FLOAT32: return decodeF32(samplePtr, endian);
    case SampleType::FLOAT64: return decodeF64(samplePtr, endian);
    }
    return 0.0;
}

} // namespace SampledDecoder

/**
 * @brief Unified sampled-data NodeData type (audio, DAQ, sensors).
 *
 * Evolution of the legacy GenericNumericData (BuiltInNodes/Library/types/
 * GenericNumericTypes.h) promoted to src/plugins/common/NodeDataTypes/
 * (REQ-SW-PL-022 AC 2). Carries a raw QByteArray buffer + a
 * SampledStreamDescriptor and decodes to per-channel normalized doubles in
 * [-1, 1] via the unified SampledDecoder convention.
 *
 * QtCore-only: this header pulls in no QtMultimedia. `AudioData` is simply a
 * SampledData with domain = "audio" — there is no separate class; the domain
 * field discriminates.
 *
 * Audio transport is intentionally NOT zero-copy: audio buffers are ~7 KB
 * (44.1 kHz stereo float32 20 ms block) and a single memcpy is sub-microsecond.
 * Zero-copy is a video-specific problem (REQ-SW-PL-020); it is not carried
 * over to audio (REQ-SW-PL-022, out of scope note).
 */
class SampledData : public QtNodes::NodeData
{
public:
    SampledData() = default;

    SampledData(QByteArray buffer, SampledStreamDescriptor descriptor)
        : m_buffer(std::move(buffer))
        , m_descriptor(std::move(descriptor))
    {
    }

    QtNodes::NodeDataType type() const override
    {
        return QtNodes::NodeDataType {"sample", "Sample"};
    }

    QByteArray &buffer() { return m_buffer; }
    const QByteArray &buffer() const { return m_buffer; }

    const SampledStreamDescriptor &descriptor() const { return m_descriptor; }
    void setDescriptor(const SampledStreamDescriptor &descriptor) { m_descriptor = descriptor; }

    /// Convenience accessors mirroring the legacy GenericNumericData API.
    int channels() const { return m_descriptor.totalChannels(); }
    double sampleRate() const { return m_descriptor.sampleRate; }
    QString domain() const { return m_descriptor.domain; }

    /**
     * @brief Decode the raw buffer into per-channel normalized values.
     *
     * outChannels is resized to the channel count; each channel vector holds
     * `buffer.size() / bytesPerFrame()` samples normalized to [-1, 1] using
     * the unified SampledDecoder convention (REQ-SW-PL-022 AC 3).
     */
    void decodeToNormalized(QVector<QVector<double>> &outChannels) const
    {
        outChannels.clear();
        const int nChannels = m_descriptor.totalChannels();
        const int frameBytes = m_descriptor.bytesPerFrame();
        if (nChannels <= 0 || frameBytes <= 0 || m_buffer.isEmpty())
            return;

        const int totalFrames = m_buffer.size() / frameBytes;
        outChannels.resize(nChannels);
        for (int ch = 0; ch < nChannels; ++ch)
            outChannels[ch].resize(totalFrames);

        const char *ptr = m_buffer.constData();
        for (int frame = 0; frame < totalFrames; ++frame) {
            for (int ch = 0; ch < nChannels; ++ch) {
                const StreamChannelDescriptor &desc = m_descriptor.channels.at(ch);
                outChannels[ch][frame] = SampledDecoder::decodeNormalizedSample(
                    ptr, desc.sampleType, m_descriptor.endianness);
                ptr += sampleTypeByteSize(desc.sampleType);
            }
        }
    }

    /**
     * @brief Decode the raw buffer into per-channel normalized float values.
     *
     * Mirrors decodeToNormalized() for the display path (REQ-SW-PL-023): same
     * channel layout, same SampledDecoder convention (32767 + clamp to [-1, 1]),
     * but decodes directly into float channels without a double intermediate
     * container.
     */
    void decodeToNormalizedF32(QVector<QVector<float>> &outChannels) const
    {
        outChannels.clear();
        const int nChannels = m_descriptor.totalChannels();
        const int frameBytes = m_descriptor.bytesPerFrame();
        if (nChannels <= 0 || frameBytes <= 0 || m_buffer.isEmpty())
            return;

        const int totalFrames = m_buffer.size() / frameBytes;
        outChannels.resize(nChannels);
        for (int ch = 0; ch < nChannels; ++ch)
            outChannels[ch].resize(totalFrames);

        const char *ptr = m_buffer.constData();
        for (int frame = 0; frame < totalFrames; ++frame) {
            for (int ch = 0; ch < nChannels; ++ch) {
                const StreamChannelDescriptor &desc = m_descriptor.channels.at(ch);
                outChannels[ch][frame] = static_cast<float>(
                    SampledDecoder::decodeNormalizedSample(
                        ptr, desc.sampleType, m_descriptor.endianness));
                ptr += sampleTypeByteSize(desc.sampleType);
            }
        }
    }

    /**
     * @brief Decode the raw buffer into per-channel PHYSICAL values.
     *
     * Mirrors decodeToNormalizedF32() in structure (same channel/frame loop,
     * same endianness handling via the SampledDecoder helpers), but the value
     * semantics are physical (REQ-SW-PL-025 AC 1):
     *
     *   value = raw × amplitudeScale + amplitudeOffset
     *
     * where `raw` is the endian-aware raw sample. There is NO normalization —
     * integer samples are NOT divided by (2^(bits-1) − 1) and unsigned samples
     * are NOT centered — and there is NO clamp to [-1, 1]; FLOAT32/FLOAT64
     * samples pass through unchanged before the scale+offset. The result may
     * fall outside [-1, 1] (e.g. ±10 V DAQ input) — that is the point.
     *
     * amplitudeScale/amplitudeOffset are read from the descriptor once per
     * call. The normalized path (decodeToNormalizedF32) is unchanged and
     * remains available for backward compatibility.
     */
    void decodeToPhysical(QVector<QVector<float>> &outChannels) const
    {
        outChannels.clear();
        const int nChannels = m_descriptor.totalChannels();
        const int frameBytes = m_descriptor.bytesPerFrame();
        if (nChannels <= 0 || frameBytes <= 0 || m_buffer.isEmpty())
            return;

        const double scale = m_descriptor.amplitudeScale;
        const double offset = m_descriptor.amplitudeOffset;

        const int totalFrames = m_buffer.size() / frameBytes;
        outChannels.resize(nChannels);
        for (int ch = 0; ch < nChannels; ++ch)
            outChannels[ch].resize(totalFrames);

        const char *ptr = m_buffer.constData();
        for (int frame = 0; frame < totalFrames; ++frame) {
            for (int ch = 0; ch < nChannels; ++ch) {
                const StreamChannelDescriptor &desc = m_descriptor.channels.at(ch);
                outChannels[ch][frame] = static_cast<float>(
                    SampledDecoder::decodeRawSample(
                        ptr, desc.sampleType, m_descriptor.endianness)
                        * scale + offset);
                ptr += sampleTypeByteSize(desc.sampleType);
            }
        }
    }

private:
    QByteArray m_buffer;
    SampledStreamDescriptor m_descriptor;
};
