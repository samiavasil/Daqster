#ifndef GENERICQDEVIOCONNECTOROBSOLETE_H
#define GENERICQDEVIOCONNECTOROBSOLETE_H

#include <memory>
#include "NodeDataModelToQIODeviceConnectorObsolete.h"
#include "QDevIOStreamConfigObsolete.h"
#include "StreamChannelObsolete.h"

/**
 * @brief General purpose QDevIO connector with metadata.
 *
 * Replaces AudioNodeQdevIoConnector as the universal connector for
 * QDevIO streams. Carries:
 * - QIODevice pointer (the data stream)
 * - QDevIOStreamConfigObsolete (optional metadata about the stream format)
 * - MixedStreamPayload (optional, for mixed multi-type streams)
 *
 * Display nodes read the metadata to auto-route to the correct view.
 * If no metadata is present, the display shows a config panel.
 *
 * @note Renamed to *_obsolete (REQ-SW-PL-023 §7) — implementation unchanged.
 *       Kept working until the QDevIO display world is deleted at the very end.
 */
class GenericQDevIoConnectorObsolete : public NodeDataModelToQIODeviceConnectorObsolete {
public:
    explicit GenericQDevIoConnectorObsolete(QtNodes::NodeDelegateModel* model);
    ~GenericQDevIoConnectorObsolete() override = default;

    // ── QIODevice ──────────────────────────────────────────────
    void setIODevice(std::shared_ptr<QIODevice> device);
    std::shared_ptr<QIODevice> ioDevice() const;

    // ── Stream Metadata ────────────────────────────────────────
    void setStreamConfig(QDevIOStreamConfigObsolete config);
    bool hasStreamConfig() const;
    QDevIOStreamConfigObsolete streamConfig() const;

    // ── Mixed Streams ──────────────────────────────────────────
    void setPayload(MixedStreamPayload payload);
    bool isMixed() const;
    MixedStreamPayload& payload();

    // ── NodeData interface ─────────────────────────────────────
    void ConnectModels(QtNodes::NodeDelegateModel* dst_model) override;
    QtNodes::NodeDataType type() const override;

private:
    std::shared_ptr<QIODevice> m_device;
    QDevIOStreamConfigObsolete m_config;
    MixedStreamPayload m_payload;
    bool m_hasConfig = false;
};

#endif // GENERICQDEVIOCONNECTOROBSOLETE_H
