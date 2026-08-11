#pragma once

#include <QString>
#include <QVector>
#include <QtGlobal>

#include <cstdint>

/**
 * @brief Sample type enumeration for sampled streams (audio, DAQ, sensors).
 *
 * Extended from the legacy GenericNumericTypes.h enum (which had only 6 types)
 * to cover 8/16/24/32-bit signed/unsigned integers and 32/64-bit floats —
 * a 16-channel DAQ with mixed sample types must be representable
 * (REQ-SW-PL-022 AC 4).
 */
enum class SampleType : uint8_t {
    INT8    = 0,   // signed 8-bit integer
    UINT8   = 1,   // unsigned 8-bit integer
    INT16   = 2,   // signed 16-bit integer
    UINT16  = 3,   // unsigned 16-bit integer
    INT24   = 4,   // signed 24-bit integer (3 bytes)
    UINT24  = 5,   // unsigned 24-bit integer (3 bytes)
    INT32   = 6,   // signed 32-bit integer
    UINT32  = 7,   // unsigned 32-bit integer
    FLOAT32 = 8,   // 32-bit IEEE 754 float
    FLOAT64 = 9,   // 64-bit IEEE 754 double
};

/// Byte order of the samples in the raw buffer.
enum class SampleEndian : uint8_t {
    LittleEndian = 0,
    BigEndian    = 1,
};

/// Size in bytes of one sample of the given type.
inline int sampleTypeByteSize(SampleType t)
{
    switch (t) {
    case SampleType::INT8:
    case SampleType::UINT8:
        return 1;
    case SampleType::INT16:
    case SampleType::UINT16:
        return 2;
    case SampleType::INT24:
    case SampleType::UINT24:
        return 3;
    case SampleType::INT32:
    case SampleType::UINT32:
    case SampleType::FLOAT32:
        return 4;
    case SampleType::FLOAT64:
        return 8;
    }
    return 0;
}

/// Human-readable name of a sample type ("int16", "float32", ...).
inline QString sampleTypeName(SampleType t)
{
    switch (t) {
    case SampleType::INT8:    return QStringLiteral("int8");
    case SampleType::UINT8:   return QStringLiteral("uint8");
    case SampleType::INT16:   return QStringLiteral("int16");
    case SampleType::UINT16:  return QStringLiteral("uint16");
    case SampleType::INT24:   return QStringLiteral("int24");
    case SampleType::UINT24:  return QStringLiteral("uint24");
    case SampleType::INT32:   return QStringLiteral("int32");
    case SampleType::UINT32:  return QStringLiteral("uint32");
    case SampleType::FLOAT32: return QStringLiteral("float32");
    case SampleType::FLOAT64: return QStringLiteral("float64");
    }
    return QStringLiteral("unknown");
}

/// Descriptor for a single channel of a sampled stream.
struct StreamChannelDescriptor {
    QString name;           // e.g., "Left", "Right", "R", "G", "B", "Ch0"
    SampleType sampleType;  // numeric type of each sample of this channel
};

/**
 * @brief Canonical descriptor for a sampled stream (audio, DAQ, sensors).
 *
 * Consolidates the legacy GenericStreamConfig (BuiltInNodes/Library/types/
 * GenericNumericTypes.h) and QDevIOStreamConfig (QDevIOStreamConfigObsolete.h)
 * into a
 * single self-describing descriptor carried by SampledData (REQ-SW-PL-022
 * AC 4). `domain` is the runtime discriminator ("audio", "vibration", "daq",
 * "ecg", ...) — a unified type with a domain field, no separate classes.
 *
 * Layout convention (unchanged from the legacy multiplexed stream):
 *   [ch0_sample0][ch1_sample0]...[chN_sample0][ch0_sample1]...
 * Each sample is sampleTypeByteSize(ch.sampleType) bytes.
 */
struct SampledStreamDescriptor {
    double sampleRate = 0.0;                 // Hz — double so sub-Hz DAQ (0.1 Hz)
                                             // is representable; audio is 44100.0
    QVector<StreamChannelDescriptor> channels; // per-channel {name, sampleType}
    SampleEndian endianness = SampleEndian::LittleEndian;
    QString unit = QStringLiteral("V");        // "V", "mV", "raw", "dB", "normalized"
    double amplitudeScale = 1.0;               // value per LSB (Y-axis labeling)
    double amplitudeOffset = 0.0;              // DC offset
    QString domain;                            // "audio", "vibration", "daq", "ecg", ...
    QString deviceId;                          // source device id (if any)
    QString sourceName;                        // source / stream name
    qint64 firstSampleTimestamp = 0;           // µs since epoch (or stream-local)

    int totalChannels() const { return channels.size(); }

    int bytesPerFrame() const
    {
        int total = 0;
        for (const StreamChannelDescriptor &ch : channels)
            total += sampleTypeByteSize(ch.sampleType);
        return total;
    }
};
