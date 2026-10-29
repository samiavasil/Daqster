#pragma once

#include "AudioCompat.h"
#include "AudioResample.h"
#include "NodeDataTypes/SampledData.h"

#include <QAudioFormat>
#include <QByteArray>
#include <QVector>
#include <QtGlobal>

#include <algorithm>
#include <cmath>
#include <cstring>

/**
 * @brief `SampledData` → `QAudioFormat` bridge for the audio output node
 * (REQ-SW-PL-076).
 *
 * Mirror image of `Sources/Video/AudioBufferToSampled.h:26-114`: that header
 * wraps a `QAudioBuffer` into `SampledData` at the capture boundary; this one
 * turns a `SampledData` descriptor into the `QAudioFormat` a `QAudioSink`
 * expects and converts a block into that format.
 *
 * Responsibilities:
 *  - descriptor → `QAudioFormat` (rate / channels / sample type), with the
 *    Qt5 (`setSampleType`/`setSampleSize`) vs Qt6 (`setSampleFormat`) difference
 *    handled through `AudioCompat`;
 *  - pick the format the device actually accepts (requested, else
 *    `preferredFormat()` — the "no silent failure" half of AC4);
 *  - convert one block: rate (linear interpolation, `AudioResample`), channel
 *    count (downmix / duplicate) and sample encoding (float/interger).
 *
 * Everything here is header-only, QtWidgets-free and pure — unit-testable
 * without an audio device. All output is little-endian; the Qt5 side of
 * `AudioCompat` is asked for `LittleEndian` explicitly and Qt6 formats are
 * host-endian (little-endian on the supported platforms).
 */
namespace AudioFormatBridge {

/// Number of bytes per sample for the format, or 0 when unsupported here.
inline int bytesPerSample(const QAudioFormat &format)
{
    const AudioCompat::SampleType type = AudioCompat::sampleType(format);
    const int bits = AudioCompat::sampleSize(format);
    if (type == AudioCompat::Float && bits == 32)
        return 4;
    if (type == AudioCompat::SignedInt && (bits == 16 || bits == 32))
        return bits / 8;
    if (type == AudioCompat::UnSignedInt && bits == 8)
        return 1;
    return 0;
}

/**
 * @brief Map a `Sampled` sample type to the Qt (compat) representation.
 *
 * @param type Sampled sample type
 * @param bits Receives the Qt sample size in bits (0 when unmappable)
 * @return Qt sample type; `AudioCompat::Unknown` when unmappable
 */
inline AudioCompat::SampleType sampleTypeFor(SampleType type, int &bits)
{
    switch (type) {
    case SampleType::INT8:    bits = 8;  return AudioCompat::SignedInt;
    case SampleType::UINT8:   bits = 8;  return AudioCompat::UnSignedInt;
    case SampleType::INT16:   bits = 16; return AudioCompat::SignedInt;
    case SampleType::UINT16:  bits = 16; return AudioCompat::UnSignedInt;
    case SampleType::INT24:   bits = 24; return AudioCompat::SignedInt;
    case SampleType::UINT24:  bits = 24; return AudioCompat::UnSignedInt;
    case SampleType::INT32:   bits = 32; return AudioCompat::SignedInt;
    case SampleType::UINT32:  bits = 32; return AudioCompat::UnSignedInt;
    case SampleType::FLOAT32: bits = 32; return AudioCompat::Float;
    case SampleType::FLOAT64: bits = 32; return AudioCompat::Float;   // Qt has no float64
    }
    bits = 0;
    return AudioCompat::Unknown;
}

/**
 * @brief Build the `QAudioFormat` described by a `SampledData` descriptor.
 *
 * Mixed per-channel sample types cannot be expressed in a single
 * `QAudioFormat`; the first channel wins and the rest are converted on the
 * way out (`convert()` encodes everything to one type anyway).
 *
 * @param descriptor Stream descriptor (rate, channels, sample type)
 * @return A valid format, or an invalid one for an unusable descriptor
 */
inline QAudioFormat formatFromDescriptor(const SampledStreamDescriptor &descriptor)
{
    if (descriptor.sampleRate <= 0.0 || descriptor.channels.isEmpty())
        return QAudioFormat();

    int bits = 0;
    const AudioCompat::SampleType compatType =
        sampleTypeFor(descriptor.channels.constFirst().sampleType, bits);
    if (compatType == AudioCompat::Unknown || bits <= 0)
        return QAudioFormat();

    QAudioFormat format;
    format.setSampleRate(int(std::lround(descriptor.sampleRate)));
    format.setChannelCount(descriptor.totalChannels());
    AudioCompat::setCodec(format, QStringLiteral("audio/pcm"));   // no-op on Qt6
    AudioCompat::setByteOrder(format, AudioCompat::LittleEndian); // no-op on Qt6
    AudioCompat::setSampleSize(format, bits);                     // order matters on Qt6
    AudioCompat::setSampleType(format, compatType);
    return format;
}

/// The format to open the device with, and why it was chosen.
struct PlaybackFormat {
    QAudioFormat format;      ///< Format to construct the sink with
    bool converted = false;   ///< true → `convert()` will change rate/type/channels
    QString reason;           ///< Human-readable explanation for `status()`

    bool isValid() const { return format.isValid(); }
};

/**
 * @brief Choose the playback format for a device (AC4, never silent).
 *
 * Asks the device first (`isFormatSupported()`); on rejection falls back to
 * `preferredFormat()` and reports that conversion is needed. A null device
 * returns the descriptor's format untouched — the caller's headless no-op path.
 *
 * @param descriptor Stream descriptor of the incoming block
 * @param device     Output device (`defaultOutputDevice()` or a chosen one)
 * @return Chosen format + reason; invalid format when neither candidate works
 */
inline PlaybackFormat resolveFormat(const SampledStreamDescriptor &descriptor,
                                    const QAudioDeviceInfo &device)
{
    PlaybackFormat result;
    result.format = formatFromDescriptor(descriptor);

    if (AudioCompat::isNull(device)) {
        result.reason = QStringLiteral("no audio output device");
        return result;
    }
    if (!result.format.isValid()) {
        result.reason = QStringLiteral("unsupported source format");
        return result;
    }
    if (AudioCompat::isFormatSupported(device, result.format)) {
        result.reason = QStringLiteral("device accepts the source format");
        return result;
    }

    const QAudioFormat preferred = AudioCompat::preferredFormat(device);
    if (!preferred.isValid()) {
        result.reason = QStringLiteral("device rejected the format and has no preferred format");
        result.format = QAudioFormat();
        return result;
    }

    result.converted = true;
    result.reason = QStringLiteral("device rejected %1 Hz / %2 ch / %3 bit → using preferred %4 Hz / %5 ch")
                        .arg(result.format.sampleRate())
                        .arg(result.format.channelCount())
                        .arg(AudioCompat::sampleSize(result.format))
                        .arg(preferred.sampleRate())
                        .arg(preferred.channelCount());
    result.format = preferred;
    return result;
}

/**
 * @brief Mix a de-interleaved buffer to @p targetChannels channels.
 *
 * - equal counts → unchanged
 * - target 1 → average of all source channels (the audio-correct downmix)
 * - target > source → channels are duplicated (`ch % sourceCount`)
 * - target < source (target > 1) → the first @p targetChannels are taken
 *
 * @param channels       Per-channel normalized samples
 * @param targetChannels Requested channel count (> 0)
 * @return Per-channel samples, or empty when the input is unusable
 */
inline QVector<QVector<float>> mixChannels(const QVector<QVector<float>> &channels,
                                           int targetChannels)
{
    if (channels.isEmpty() || targetChannels <= 0)
        return {};
    if (channels.size() == targetChannels)
        return channels;

    const int frames = channels.constFirst().size();
    QVector<QVector<float>> out(targetChannels, QVector<float>(frames, 0.0f));

    if (targetChannels == 1) {
        for (const QVector<float> &channel : channels)
            for (int i = 0; i < frames && i < channel.size(); ++i)
                out[0][i] += channel.at(i);
        const float inv = 1.0f / float(channels.size());
        for (float &value : out[0])
            value *= inv;
        return out;
    }

    for (int channel = 0; channel < targetChannels; ++channel) {
        const QVector<float> &source = channels.at(channel % channels.size());
        for (int i = 0; i < frames && i < source.size(); ++i)
            out[channel][i] = source.at(i);
    }
    return out;
}

/// Clamp to [-1, 1] — every encoder below assumes normalized input.
inline float clamped(float value)
{
    return std::max(-1.0f, std::min(1.0f, value));
}

/// Interleave and encode as 32-bit float (little-endian).
inline QByteArray encodeF32(const QVector<QVector<float>> &channels)
{
    const int frames = channels.constFirst().size();
    const int count = channels.size();
    QByteArray out(frames * count * 4, Qt::Uninitialized);
    char *dst = out.data();
    for (int frame = 0; frame < frames; ++frame) {
        for (int channel = 0; channel < count; ++channel, dst += 4) {
            const float value = clamped(channels.at(channel).at(frame));
            quint32 bits = 0;
            std::memcpy(&bits, &value, sizeof(bits));
            dst[0] = char(bits & 0xFF);
            dst[1] = char((bits >> 8) & 0xFF);
            dst[2] = char((bits >> 16) & 0xFF);
            dst[3] = char((bits >> 24) & 0xFF);
        }
    }
    return out;
}

/// Interleave and encode as signed 16-bit (little-endian).
inline QByteArray encodeS16(const QVector<QVector<float>> &channels)
{
    const int frames = channels.constFirst().size();
    const int count = channels.size();
    QByteArray out(frames * count * 2, Qt::Uninitialized);
    char *dst = out.data();
    for (int frame = 0; frame < frames; ++frame) {
        for (int channel = 0; channel < count; ++channel, dst += 2) {
            const float value = clamped(channels.at(channel).at(frame));
            const qint16 sample = qint16(std::lround(double(value) * 32767.0));
            dst[0] = char(quint8(sample & 0xFF));
            dst[1] = char(quint8((sample >> 8) & 0xFF));
        }
    }
    return out;
}

/// Interleave and encode as signed 32-bit (little-endian).
inline QByteArray encodeS32(const QVector<QVector<float>> &channels)
{
    const int frames = channels.constFirst().size();
    const int count = channels.size();
    QByteArray out(frames * count * 4, Qt::Uninitialized);
    char *dst = out.data();
    for (int frame = 0; frame < frames; ++frame) {
        for (int channel = 0; channel < count; ++channel, dst += 4) {
            const float value = clamped(channels.at(channel).at(frame));
            const qint32 sample = qint32(std::llround(double(value) * 2147483647.0));
            for (int byte = 0; byte < 4; ++byte)
                dst[byte] = char(quint8((sample >> (8 * byte)) & 0xFF));
        }
    }
    return out;
}

/// Interleave and encode as unsigned 8-bit (centered at 128).
inline QByteArray encodeU8(const QVector<QVector<float>> &channels)
{
    const int frames = channels.constFirst().size();
    const int count = channels.size();
    QByteArray out(frames * count, Qt::Uninitialized);
    char *dst = out.data();
    for (int frame = 0; frame < frames; ++frame) {
        for (int channel = 0; channel < count; ++channel) {
            const float value = clamped(channels.at(channel).at(frame));
            *dst++ = char(quint8(std::lround(double(value) * 127.0) + 128));
        }
    }
    return out;
}

/**
 * @brief Interleave and encode normalized channels into the target format.
 *
 * @param channels Per-channel normalized samples (same length)
 * @param format   Target `QAudioFormat` (already accepted by the device)
 * @return Little-endian PCM bytes, or empty for an unsupported encoding
 */
inline QByteArray encode(const QVector<QVector<float>> &channels, const QAudioFormat &format)
{
    if (channels.isEmpty() || channels.constFirst().isEmpty())
        return {};

    const AudioCompat::SampleType type = AudioCompat::sampleType(format);
    const int bits = AudioCompat::sampleSize(format);
    if (type == AudioCompat::Float && bits == 32)
        return encodeF32(channels);
    if (type == AudioCompat::SignedInt && bits == 16)
        return encodeS16(channels);
    if (type == AudioCompat::SignedInt && bits == 32)
        return encodeS32(channels);
    if (type == AudioCompat::UnSignedInt && bits == 8)
        return encodeU8(channels);
    return {};                                          // 24-bit / uint16 / ... unsupported
}

/**
 * @brief Convert one block of `SampledData` into the target device format.
 *
 * Decodes to normalized float (the unified convention of `SampledData`),
 * resamples when the rates differ, mixes to the target channel count and
 * encodes to the target sample type.
 *
 * @param data   Incoming block (any `SampleType`, any channel count)
 * @param format Target format returned by `resolveFormat()`
 * @param error  Optional; receives a description of the failure
 * @return PCM bytes ready for `QAudioSink::start()::write()`, empty on failure
 */
inline QByteArray convert(const SampledData &data,
                          const QAudioFormat &format,
                          QString *error = nullptr)
{
    // Structural check instead of QAudioFormat::isValid(): on Qt5 isValid()
    // additionally requires a codec string, which a caller-assembled format
    // may legitimately omit (Qt6 has no codec at all). What we actually need
    // is a rate, a channel count and an encoding we can write.
    const int sampleBytes = bytesPerSample(format);
    if (format.sampleRate() <= 0 || format.channelCount() <= 0 || sampleBytes <= 0) {
        if (error)
            *error = QStringLiteral("invalid target format (%1 Hz / %2 ch / %3 bit)")
                         .arg(format.sampleRate())
                         .arg(format.channelCount())
                         .arg(AudioCompat::sampleSize(format));
        return {};
    }

    QVector<QVector<float>> channels;
    data.decodeToNormalizedF32(channels);
    if (channels.isEmpty()) {
        if (error)
            *error = QStringLiteral("empty or undecodable block");
        return {};
    }

    const double sourceRate = data.sampleRate() > 0.0 ? data.sampleRate()
                                                      : double(format.sampleRate());
    const double targetRate = double(format.sampleRate());
    if (sourceRate > 0.0 && targetRate > 0.0 && !qFuzzyCompare(sourceRate, targetRate))
        channels = AudioResample::resampleChannels(channels, sourceRate, targetRate);

    const QVector<QVector<float>> mixed = mixChannels(channels, format.channelCount());
    const QByteArray pcm = encode(mixed, format);
    if (pcm.isEmpty() && error)
        *error = QStringLiteral("unsupported sample encoding (%1 bit)")
                     .arg(AudioCompat::sampleSize(format));
    return pcm;
}

} // namespace AudioFormatBridge
