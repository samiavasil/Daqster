#ifndef DEMUXNODEOBSOLETE_H
#define DEMUXNODEOBSOLETE_H

#include <QtNodes/NodeDelegateModel>
#include <QVector>

/**
 * @brief Demultiplexes a mixed QDevIO stream into typed outputs.
 *
 * Input: {"QDevIO", "IO"} (mixed stream with multiple channel types)
 * Output: N ports, one per channel type (Audio, Sensor, etc.)
 *
 * Reads MixedStreamPayload from the connector and routes each
 * channel to its corresponding output port.
 *
 * @note Renamed to *_obsolete (REQ-SW-PL-023 §7) — implementation unchanged.
 *       Kept working until the QDevIO display world is deleted at the very end.
 *       Registered under its new name AND aliased under the old "DemuxNode" key
 *       so old saved graphs still load.
 */
class DemuxNodeObsolete : public QtNodes::NodeDelegateModel
{
    Q_OBJECT

public:
    DemuxNodeObsolete();
    ~DemuxNodeObsolete() override;

    QString caption() const override
    { return QStringLiteral("Demux (obsolete)"); }

    bool captionVisible() const override
    { return false; }

    QString name() const override
    { return QStringLiteral("DemuxNodeObsolete"); }

    QJsonObject save() const override;

    unsigned int nPorts(QtNodes::PortType portType) const override;

    QtNodes::NodeDataType dataType(QtNodes::PortType portType,
                                   QtNodes::PortIndex portIndex) const override;

    std::shared_ptr<QtNodes::NodeData> outData(QtNodes::PortIndex const port) override;

    void setInData(std::shared_ptr<QtNodes::NodeData> data,
                   QtNodes::PortIndex const portIndex) override;

    QWidget* embeddedWidget() override;

    QtNodes::ConnectionPolicy portConnectionPolicy(
        QtNodes::PortType portType, QtNodes::PortIndex portIndex) const override;

private:
    struct OutputPort {
        QString type;
        std::shared_ptr<QtNodes::NodeData> data;
    };

    QVector<OutputPort> m_outputs;
    std::shared_ptr<QtNodes::NodeData> m_input;
};

#endif // DEMUXNODEOBSOLETE_H
