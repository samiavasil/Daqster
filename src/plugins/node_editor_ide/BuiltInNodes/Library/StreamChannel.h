#ifndef STREAMCHANNEL_H
#define STREAMCHANNEL_H

#include <QString>
#include <QVariantMap>
#include <QVector>
#include <QSharedPointer>

class IStreamDecoder;

/**
 * @brief A single channel in a stream, with its decoder and metadata.
 *
 * Used in MixedStreamPayload to describe one channel in a multi-type stream.
 */
struct StreamChannel {
    QString type;                              // "audio", "sensor", "video"
    QSharedPointer<IStreamDecoder> decoder;    // decoder for this channel
    int channelCount = 0;                      // channels in this sub-stream
    QVariantMap metadata;                      // domain-specific config
};

/**
 * @brief Payload carrying one or more typed stream channels.
 *
 * Supports both single-type streams (N=1) and mixed streams (N>1).
 * Display nodes use extractChannel() to get only the type they need.
 */
struct MixedStreamPayload {
    QVector<StreamChannel> channels;

    bool isSingleType() const { return channels.size() == 1; }

    QString singleType() const {
        return channels.size() == 1 ? channels.first().type : "mixed";
    }

    StreamChannel* findChannel(const QString& type) {
        for (auto& ch : channels)
            if (ch.type == type) return &ch;
        return nullptr;
    }

    StreamChannel extractChannel(const QString& type) {
        for (int i = 0; i < channels.size(); ++i) {
            if (channels[i].type == type) {
                StreamChannel result = std::move(channels[i]);
                channels.remove(i);
                return result;
            }
        }
        return {};
    }
};

#endif // STREAMCHANNEL_H
