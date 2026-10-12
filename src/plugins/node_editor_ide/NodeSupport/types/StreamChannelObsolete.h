#ifndef STREAMCHANNELOBSOLETE_H
#define STREAMCHANNELOBSOLETE_H

#include <QString>
#include <QVariantMap>
#include <QVector>
#include <QSharedPointer>

class IStreamDecoderObsolete;

/**
 * @brief A single channel in a stream, with its decoder and metadata.
 *
 * Used in MixedStreamPayload to describe one channel in a multi-type stream.
 *
 * @note Renamed to *_obsolete (REQ-SW-PL-023 §7) — implementation unchanged.
 */
struct StreamChannelObsolete {
    QString type;                              // "audio", "sensor", "video"
    QSharedPointer<IStreamDecoderObsolete> decoder;    // decoder for this channel
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
    QVector<StreamChannelObsolete> channels;

    bool isSingleType() const { return channels.size() == 1; }

    QString singleType() const {
        return channels.size() == 1 ? channels.first().type : "mixed";
    }

    StreamChannelObsolete* findChannel(const QString& type) {
        for (auto& ch : channels)
            if (ch.type == type) return &ch;
        return nullptr;
    }

    StreamChannelObsolete extractChannel(const QString& type) {
        for (int i = 0; i < channels.size(); ++i) {
            if (channels[i].type == type) {
                StreamChannelObsolete result = std::move(channels[i]);
                channels.remove(i);
                return result;
            }
        }
        return {};
    }
};

#endif // STREAMCHANNELOBSOLETE_H
