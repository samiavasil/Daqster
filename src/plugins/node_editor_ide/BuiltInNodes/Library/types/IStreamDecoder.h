#ifndef ISTREAMDECODER_H
#define ISTREAMDECODER_H

#include <QByteArray>
#include <QVector>
#include <QString>

/**
 * @brief Interface for decoding raw QDevIO stream bytes into normalized samples.
 *
 * Each stream type (audio, sensor, SDR) provides its own implementation.
 * The display nodes use this interface to decode incoming data without
 * knowing the specific format.
 *
 * Output is always normalized to [-1.0, 1.0] range per channel.
 */
class IStreamDecoder {
public:
    virtual ~IStreamDecoder() = default;

    /**
     * @brief Decode raw bytes from QDevIO stream into per-channel samples.
     * @param raw Raw byte data from QIODevice
     * @param outChannels outChannels[channel][sample] = decoded value in [-1.0, 1.0]
     * @return Number of samples decoded per channel
     */
    virtual int decode(const QByteArray& raw,
                       QVector<QVector<double>>& outChannels) const = 0;

    /** @brief Number of channels in this stream */
    virtual int channels() const = 0;

    /** @brief Bytes per frame (all channels combined) */
    virtual int bytesPerFrame() const = 0;

    /** @brief Stream type identifier: "audio", "sensor", "sdr", etc. */
    virtual QString streamType() const = 0;
};

#endif // ISTREAMDECODER_H
