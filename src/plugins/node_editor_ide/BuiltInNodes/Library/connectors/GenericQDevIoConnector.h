#ifndef GENERICQDEVIOCONNECTOR_H
#define GENERICQDEVIOCONNECTOR_H

#include <memory>
#include "NodeDataModelToQIODeviceConnector.h"
#include "QDevIOStreamConfig.h"
#include "StreamChannel.h"

/**
 * @brief General purpose QDevIO connector with metadata.
 *
 * Replaces AudioNodeQdevIoConnector as the universal connector for
 * QDevIO streams. Carries:
 * - QIODevice pointer (the data stream)
 * - QDevIOStreamConfig (optional metadata about the stream format)
 * - MixedStreamPayload (optional, for mixed multi-type streams)
 *
 * Display nodes read the metadata to auto-route to the correct view.
 * If no metadata is present, the display shows a config panel.
 */
class GenericQDevIoConnector : public NodeDataModelToQIODeviceConnector {
public:
    explicit GenericQDevIoConnector(QtNodes::NodeDelegateModel* model);
    ~GenericQDevIoConnector() override = default;

    // ── QIODevice ──────────────────────────────────────────────
    void setIODevice(std::shared_ptr<QIODevice> device);
    std::shared_ptr<QIODevice> ioDevice() const;

    // ── Stream Metadata ────────────────────────────────────────
    void setStreamConfig(QDevIOStreamConfig config);
    bool hasStreamConfig() const;
    QDevIOStreamConfig streamConfig() const;

    // ── Mixed Streams ──────────────────────────────────────────
    void setPayload(MixedStreamPayload payload);
    bool isMixed() const;
    MixedStreamPayload& payload();

    // ── NodeData interface ─────────────────────────────────────
    void ConnectModels(QtNodes::NodeDelegateModel* dst_model) override;
    QtNodes::NodeDataType type() const override;

private:
    std::shared_ptr<QIODevice> m_device;
    QDevIOStreamConfig m_config;
    MixedStreamPayload m_payload;
    bool m_hasConfig = false;
};

#endif // GENERICQDEVIOCONNECTOR_H
