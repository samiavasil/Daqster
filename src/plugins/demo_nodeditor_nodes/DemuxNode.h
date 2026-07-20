#ifndef DEMUXNODE_H
#define DEMUXNODE_H

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
 */
class DemuxNode : public QtNodes::NodeDelegateModel
{
    Q_OBJECT

public:
    DemuxNode();
    ~DemuxNode() override;

    QString caption() const override
    { return QStringLiteral("Demux"); }

    bool captionVisible() const override
    { return false; }

    QString name() const override
    { return QStringLiteral("DemuxNode"); }

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

#endif // DEMUXNODE_H
