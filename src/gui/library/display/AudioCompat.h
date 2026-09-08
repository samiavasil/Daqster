#pragma once

#include <QtGlobal>
#include <QtMultimedia/QAudio>
#include <QtMultimedia/QAudioFormat>

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QtMultimedia/QAudioDevice>
#include <QtMultimedia/QAudioSource>
#include <QtMultimedia/QMediaDevices>
#include <QSet>
#include <QSysInfo>
#include <QStringList>

using QAudioDeviceInfo = QAudioDevice;
#else
#include <QtMultimedia/QAudioDeviceInfo>
#include <QtMultimedia/QAudioInput>
#include <QStringList>
#endif

namespace AudioCompat {

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)

using AudioInput = QAudioSource;
using Mode = QAudioDevice::Mode;
static constexpr Mode AudioInputMode = QAudioDevice::Input;
static constexpr Mode AudioOutputMode = QAudioDevice::Output;

enum SampleType {
    Unknown = 0,
    SignedInt,
    UnSignedInt,
    Float
};

enum Endian {
    LittleEndian = 0,
    BigEndian
};

inline QString deviceName(const QAudioDeviceInfo &device)
{
    return device.description();
}

inline QList<QAudioDeviceInfo> availableDevices(Mode mode)
{
    return mode == AudioInputMode ? QMediaDevices::audioInputs() : QMediaDevices::audioOutputs();
}

inline QAudioDeviceInfo defaultInputDevice()
{
    return QMediaDevices::defaultAudioInput();
}

inline bool isNull(const QAudioDeviceInfo &device)
{
    return device.isNull();
}

inline bool isFormatSupported(const QAudioDeviceInfo &device, const QAudioFormat &format)
{
    return device.isFormatSupported(format);
}

inline QAudioFormat preferredFormat(const QAudioDeviceInfo &device)
{
    return device.preferredFormat();
}

inline QList<int> supportedChannelCounts(const QAudioDeviceInfo &device)
{
    QList<int> values;
    for (int channels = device.minimumChannelCount(); channels <= device.maximumChannelCount(); ++channels)
        values.append(channels);
    if (values.isEmpty() && device.preferredFormat().channelCount() > 0)
        values.append(device.preferredFormat().channelCount());
    return values;
}

inline QList<int> supportedSampleRates(const QAudioDeviceInfo &device)
{
    QSet<int> values;
    if (device.minimumSampleRate() > 0)
        values.insert(device.minimumSampleRate());
    if (device.preferredFormat().sampleRate() > 0)
        values.insert(device.preferredFormat().sampleRate());
    if (device.maximumSampleRate() > 0)
        values.insert(device.maximumSampleRate());
    return values.values();
}

inline int sampleSize(const QAudioFormat &format)
{
    switch (format.sampleFormat()) {
    case QAudioFormat::UInt8:
        return 8;
    case QAudioFormat::Int16:
        return 16;
    case QAudioFormat::Int32:
    case QAudioFormat::Float:
        return 32;
    case QAudioFormat::Unknown:
    default:
        return 0;
    }
}

inline SampleType sampleType(const QAudioFormat &format)
{
    switch (format.sampleFormat()) {
    case QAudioFormat::UInt8:
        return UnSignedInt;
    case QAudioFormat::Int16:
    case QAudioFormat::Int32:
        return SignedInt;
    case QAudioFormat::Float:
        return Float;
    case QAudioFormat::Unknown:
    default:
        return Unknown;
    }
}

inline Endian byteOrder(const QAudioFormat &)
{
    return QSysInfo::ByteOrder == QSysInfo::BigEndian ? BigEndian : LittleEndian;
}

inline QString codec(const QAudioFormat &)
{
    return QStringLiteral("audio/pcm");
}

inline QAudioFormat::SampleFormat toSampleFormat(SampleType type, int bits)
{
    switch (type) {
    case UnSignedInt:
        return QAudioFormat::UInt8;
    case SignedInt:
        if (bits <= 16)
            return QAudioFormat::Int16;
        return QAudioFormat::Int32;
    case Float:
        return QAudioFormat::Float;
    case Unknown:
    default:
        return QAudioFormat::Unknown;
    }
}

inline void setCodec(QAudioFormat &, const QString &)
{}

inline void setByteOrder(QAudioFormat &, Endian)
{}

inline void setSampleSize(QAudioFormat &format, int bits)
{
    SampleType type = sampleType(format);
    if (type == Unknown)
        type = bits <= 8 ? UnSignedInt : SignedInt;
    format.setSampleFormat(toSampleFormat(type, bits));
}

inline void setSampleType(QAudioFormat &format, SampleType type)
{
    int bits = sampleSize(format);
    if (bits <= 0)
        bits = (type == Float) ? 32 : (type == UnSignedInt ? 8 : 16);
    format.setSampleFormat(toSampleFormat(type, bits));
}

inline QList<int> supportedSampleSizes(const QAudioDeviceInfo &device)
{
    QSet<int> values;
    for (QAudioFormat::SampleFormat format : device.supportedSampleFormats()) {
        QAudioFormat audioFormat;
        audioFormat.setSampleFormat(format);
        values.insert(sampleSize(audioFormat));
    }
    if (values.isEmpty() && device.preferredFormat().isValid())
        values.insert(sampleSize(device.preferredFormat()));
    values.remove(0);
    return values.values();
}

inline QList<SampleType> supportedSampleTypes(const QAudioDeviceInfo &device)
{
    QSet<int> values;
    for (QAudioFormat::SampleFormat format : device.supportedSampleFormats()) {
        QAudioFormat audioFormat;
        audioFormat.setSampleFormat(format);
        values.insert(static_cast<int>(sampleType(audioFormat)));
    }
    if (values.isEmpty() && device.preferredFormat().isValid())
        values.insert(static_cast<int>(sampleType(device.preferredFormat())));

    QList<SampleType> result;
    for (int value : values)
        result.append(static_cast<SampleType>(value));
    return result;
}

inline QStringList supportedCodecs(const QAudioDeviceInfo &)
{
    return {QStringLiteral("audio/pcm")};
}

inline QList<Endian> supportedByteOrders(const QAudioDeviceInfo &)
{
    return {byteOrder(QAudioFormat())};
}

#else

using AudioInput = QAudioInput;
using Mode = QAudio::Mode;
static constexpr Mode AudioInputMode = QAudio::AudioInput;
static constexpr Mode AudioOutputMode = QAudio::AudioOutput;

using SampleType = QAudioFormat::SampleType;
using Endian = QAudioFormat::Endian;
static constexpr SampleType Unknown = QAudioFormat::Unknown;
static constexpr SampleType SignedInt = QAudioFormat::SignedInt;
static constexpr SampleType UnSignedInt = QAudioFormat::UnSignedInt;
static constexpr SampleType Float = QAudioFormat::Float;
static constexpr Endian LittleEndian = QAudioFormat::LittleEndian;
static constexpr Endian BigEndian = QAudioFormat::BigEndian;

inline QString deviceName(const QAudioDeviceInfo &device)
{
    return device.deviceName();
}

inline QList<QAudioDeviceInfo> availableDevices(Mode mode)
{
    return QAudioDeviceInfo::availableDevices(mode);
}

inline QAudioDeviceInfo defaultInputDevice()
{
    return QAudioDeviceInfo::defaultInputDevice();
}

inline bool isNull(const QAudioDeviceInfo &device)
{
    return device.isNull();
}

inline bool isFormatSupported(const QAudioDeviceInfo &device, const QAudioFormat &format)
{
    return device.isFormatSupported(format);
}

inline QAudioFormat preferredFormat(const QAudioDeviceInfo &device)
{
    return device.preferredFormat();
}

inline QList<int> supportedChannelCounts(const QAudioDeviceInfo &device)
{
    return device.supportedChannelCounts();
}

inline QList<int> supportedSampleRates(const QAudioDeviceInfo &device)
{
    return device.supportedSampleRates();
}

inline QList<int> supportedSampleSizes(const QAudioDeviceInfo &device)
{
    return device.supportedSampleSizes();
}

inline QList<SampleType> supportedSampleTypes(const QAudioDeviceInfo &device)
{
    return device.supportedSampleTypes();
}

inline QStringList supportedCodecs(const QAudioDeviceInfo &device)
{
    return device.supportedCodecs();
}

inline QList<Endian> supportedByteOrders(const QAudioDeviceInfo &device)
{
    return device.supportedByteOrders();
}

inline int sampleSize(const QAudioFormat &format)
{
    return format.sampleSize();
}

inline SampleType sampleType(const QAudioFormat &format)
{
    return format.sampleType();
}

inline Endian byteOrder(const QAudioFormat &format)
{
    return format.byteOrder();
}

inline QString codec(const QAudioFormat &format)
{
    return format.codec();
}

inline void setCodec(QAudioFormat &format, const QString &value)
{
    format.setCodec(value);
}

inline void setByteOrder(QAudioFormat &format, Endian value)
{
    format.setByteOrder(value);
}

inline void setSampleSize(QAudioFormat &format, int bits)
{
    format.setSampleSize(bits);
}

inline void setSampleType(QAudioFormat &format, SampleType type)
{
    format.setSampleType(type);
}

#endif

} // namespace AudioCompat