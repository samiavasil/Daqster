#pragma once

#include "SampledData.h"

#include <QAudioBuffer>
#include <QAudioFormat>

#include <memory>

/**
 * @brief QAudioBuffer → SampledData capture glue for video source nodes.
 *
 * Lives at the source boundary (REQ-SW-PL-022 §4): the handler wraps the
 * decoded QAudioBuffer into SampledData (domain = "audio") with a single
 * QByteArray copy (~7 KB at 44.1 kHz stereo float32 20 ms — sub-microsecond,
 * no zero-copy needed for audio, documented in REQ-SW-PL-022 §3).
 *
 * No sample conversion happens here — SampledData::decodeToNormalized() (or
 * the unified SampledDecoder) decodes lazily on the consumer side.
 *
 * Note the Qt5/Qt6 QAudioFormat API difference handled below:
 *  - Qt6: QAudioFormat::sampleFormat() (UInt8/Int16/Int32/Float) — no byte
 *    order / sample size accessors (host-endian, size implied by format);
 *  - Qt5: QAudioFormat::sampleType() + sampleSize() + byteOrder().
 */
namespace AudioBufferToSampled {

/// Map a QAudioFormat to the unified SampleType (Qt5/Qt6 aware).
inline SampleType sampleTypeFromFormat(const QAudioFormat &format)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    switch (format.sampleFormat()) {
    case QAudioFormat::UInt8: return SampleType::UINT8;
    case QAudioFormat::Int16: return SampleType::INT16;
    case QAudioFormat::Int32: return SampleType::INT32;
    case QAudioFormat::Float: return SampleType::FLOAT32;
    default:                  return SampleType::FLOAT32;
    }
#else
    switch (format.sampleType()) {
    case QAudioFormat::SignedInt:
        switch (format.sampleSize()) {
        case 8:  return SampleType::INT8;
        case 24: return SampleType::INT24;
        case 32: return SampleType::INT32;
        default: return SampleType::INT16;
        }
    case QAudioFormat::UnSignedInt:
        switch (format.sampleSize()) {
        case 8:  return SampleType::UINT8;
        case 24: return SampleType::UINT24;
        case 32: return SampleType::UINT32;
        default: return SampleType::UINT16;
        }
    case QAudioFormat::Float:
        return format.sampleSize() == 64 ? SampleType::FLOAT64 : SampleType::FLOAT32;
    default:
        return SampleType::FLOAT32;
    }
#endif
}

/// Build a SampledStreamDescriptor (domain = "audio") from a QAudioFormat.
inline SampledStreamDescriptor descriptorFromFormat(const QAudioFormat &format,
                                                    const QString &sourceName = QString())
{
    SampledStreamDescriptor desc;
    desc.domain = QStringLiteral("audio");
    desc.sampleRate = format.sampleRate();
    desc.unit = QStringLiteral("normalized");
    desc.amplitudeScale = 1.0;
    desc.amplitudeOffset = 0.0;
    desc.sourceName = sourceName;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    // Qt6 formats are host-endian (little-endian on the supported platforms).
    desc.endianness = SampleEndian::LittleEndian;
#else
    desc.endianness = (format.byteOrder() == QAudioFormat::BigEndian)
                          ? SampleEndian::BigEndian
                          : SampleEndian::LittleEndian;
#endif

    const int nCh = qMax(1, format.channelCount());
    const SampleType sampleType = sampleTypeFromFormat(format);
    desc.channels.reserve(nCh);
    for (int i = 0; i < nCh; ++i) {
        desc.channels.append(
            StreamChannelDescriptor {QStringLiteral("Ch%1").arg(i), sampleType});
    }
    return desc;
}

/**
 * @brief Wrap a decoded audio buffer into SampledData (copy only).
 *
 * Returns nullptr for invalid / empty buffers — an empty QAudioBuffer at
 * end-of-stream is flushed/ignored, no EOS type is emitted (REQ-SW-PL-022 §4).
 */
inline std::shared_ptr<SampledData> wrapBuffer(const QAudioBuffer &buffer,
                                               const QString &sourceName = QString())
{
    if (!buffer.isValid() || buffer.byteCount() <= 0)
        return nullptr;

    return std::make_shared<SampledData>(
        QByteArray(reinterpret_cast<const char *>(buffer.constData()),
                   static_cast<int>(buffer.byteCount())),
        descriptorFromFormat(buffer.format(), sourceName));
}

} // namespace AudioBufferToSampled
