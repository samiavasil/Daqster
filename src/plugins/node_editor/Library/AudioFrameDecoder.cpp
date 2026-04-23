#include "AudioFrameDecoder.h"

#include <QtCore/QString>
#include <cstring>

namespace {

inline qreal clampUnit(qreal x)
{
    if (x > 1.0) return 1.0;
    if (x < -1.0) return -1.0;
    return x;
}

// --- Signed integer decoders ------------------------------------------------

qreal decodeS8(const char *p)
{
    return clampUnit(qreal(*reinterpret_cast<const qint8 *>(p)) / qreal(127.0));
}

qreal decodeS16LE(const char *p)
{
    const qint16 v = static_cast<qint16>(
        static_cast<quint8>(p[0]) |
        (static_cast<quint8>(p[1]) << 8));
    return clampUnit(qreal(v) / qreal(32767.0));
}

qreal decodeS16BE(const char *p)
{
    const qint16 v = static_cast<qint16>(
        static_cast<quint8>(p[1]) |
        (static_cast<quint8>(p[0]) << 8));
    return clampUnit(qreal(v) / qreal(32767.0));
}

qreal decodeS24LE(const char *p)
{
    qint32 v = static_cast<quint8>(p[0]) |
              (static_cast<quint8>(p[1]) << 8) |
              (static_cast<qint8>(p[2]) << 16);
    return clampUnit(qreal(v) / qreal(8388607.0));
}

qreal decodeS24BE(const char *p)
{
    qint32 v = static_cast<quint8>(p[2]) |
              (static_cast<quint8>(p[1]) << 8) |
              (static_cast<qint8>(p[0]) << 16);
    return clampUnit(qreal(v) / qreal(8388607.0));
}

qreal decodeS32LE(const char *p)
{
    const qint32 v = static_cast<qint32>(
        static_cast<quint8>(p[0]) |
        (static_cast<quint8>(p[1]) << 8) |
        (static_cast<quint8>(p[2]) << 16) |
        (static_cast<quint8>(p[3]) << 24));
    return clampUnit(qreal(v) / qreal(2147483647.0));
}

qreal decodeS32BE(const char *p)
{
    const qint32 v = static_cast<qint32>(
        static_cast<quint8>(p[3]) |
        (static_cast<quint8>(p[2]) << 8) |
        (static_cast<quint8>(p[1]) << 16) |
        (static_cast<quint8>(p[0]) << 24));
    return clampUnit(qreal(v) / qreal(2147483647.0));
}

// --- Unsigned integer decoders ---------------------------------------------

qreal decodeU8(const char *p)
{
    const int v = static_cast<quint8>(p[0]);
    return clampUnit(qreal(v - 128) / qreal(127.0));
}

qreal decodeU16LE(const char *p)
{
    const int v = static_cast<int>(
        static_cast<quint8>(p[0]) |
        (static_cast<quint8>(p[1]) << 8));
    return clampUnit(qreal(v - 32768) / qreal(32767.0));
}

qreal decodeU16BE(const char *p)
{
    const int v = static_cast<int>(
        static_cast<quint8>(p[1]) |
        (static_cast<quint8>(p[0]) << 8));
    return clampUnit(qreal(v - 32768) / qreal(32767.0));
}

qreal decodeU24LE(const char *p)
{
    const int v = static_cast<int>(
        static_cast<quint8>(p[0]) |
        (static_cast<quint8>(p[1]) << 8) |
        (static_cast<quint8>(p[2]) << 16));
    return clampUnit(qreal(v - 8388608) / qreal(8388607.0));
}

qreal decodeU24BE(const char *p)
{
    const int v = static_cast<int>(
        static_cast<quint8>(p[2]) |
        (static_cast<quint8>(p[1]) << 8) |
        (static_cast<quint8>(p[0]) << 16));
    return clampUnit(qreal(v - 8388608) / qreal(8388607.0));
}

qreal decodeU32LE(const char *p)
{
    const quint32 v = static_cast<quint32>(
        static_cast<quint8>(p[0]) |
        (static_cast<quint8>(p[1]) << 8) |
        (static_cast<quint8>(p[2]) << 16) |
        (static_cast<quint8>(p[3]) << 24));
    return clampUnit(qreal(static_cast<qint64>(v) - 2147483648LL) / qreal(2147483647.0));
}

qreal decodeU32BE(const char *p)
{
    const quint32 v = static_cast<quint32>(
        static_cast<quint8>(p[3]) |
        (static_cast<quint8>(p[2]) << 8) |
        (static_cast<quint8>(p[1]) << 16) |
        (static_cast<quint8>(p[0]) << 24));
    return clampUnit(qreal(static_cast<qint64>(v) - 2147483648LL) / qreal(2147483647.0));
}

// --- Float decoders ---------------------------------------------------------

qreal decodeF32LE(const char *p)
{
    quint32 bits = static_cast<quint32>(
        static_cast<quint8>(p[0]) |
        (static_cast<quint8>(p[1]) << 8) |
        (static_cast<quint8>(p[2]) << 16) |
        (static_cast<quint8>(p[3]) << 24));
    float value;
    std::memcpy(&value, &bits, sizeof(value));
    if (value > 1.0F) value = 1.0F;
    if (value < -1.0F) value = -1.0F;
    return qreal(value);
}

qreal decodeF32BE(const char *p)
{
    quint32 bits = static_cast<quint32>(
        static_cast<quint8>(p[3]) |
        (static_cast<quint8>(p[2]) << 8) |
        (static_cast<quint8>(p[1]) << 16) |
        (static_cast<quint8>(p[0]) << 24));
    float value;
    std::memcpy(&value, &bits, sizeof(value));
    if (value > 1.0F) value = 1.0F;
    if (value < -1.0F) value = -1.0F;
    return qreal(value);
}

} // namespace

AudioFrameDecoder::AudioFrameDecoder()
{
}

bool AudioFrameDecoder::configure(const QAudioFormat &format)
{
    m_decoder = nullptr;
    m_channels = qMax(1, format.channelCount());
    m_bytesPerSample = qMax(1, format.sampleSize() / 8);

    if (m_bytesPerSample > 4) {
        return false;
    }

    if (!format.codec().isEmpty() && format.codec() != QStringLiteral("audio/pcm")) {
        return false;
    }

    const bool isBig = format.byteOrder() == QAudioFormat::BigEndian;

    switch (format.sampleType()) {
    case QAudioFormat::SignedInt:
        switch (format.sampleSize()) {
        case 8:  m_decoder = decodeS8; break;
        case 16: m_decoder = isBig ? decodeS16BE : decodeS16LE; break;
        case 24: m_decoder = isBig ? decodeS24BE : decodeS24LE; break;
        case 32: m_decoder = isBig ? decodeS32BE : decodeS32LE; break;
        default: break;
        }
        break;
    case QAudioFormat::UnSignedInt:
        switch (format.sampleSize()) {
        case 8:  m_decoder = decodeU8; break;
        case 16: m_decoder = isBig ? decodeU16BE : decodeU16LE; break;
        case 24: m_decoder = isBig ? decodeU24BE : decodeU24LE; break;
        case 32: m_decoder = isBig ? decodeU32BE : decodeU32LE; break;
        default: break;
        }
        break;
    case QAudioFormat::Float:
        if (format.sampleSize() == 32) {
            m_decoder = isBig ? decodeF32BE : decodeF32LE;
        }
        break;
    default:
        break;
    }

    return m_decoder != nullptr;
}

qreal AudioFrameDecoder::decodeNormalizedSample(const char *samplePtr) const
{
    if (m_decoder == nullptr) {
        return 0.0;
    }
    return m_decoder(samplePtr);
}