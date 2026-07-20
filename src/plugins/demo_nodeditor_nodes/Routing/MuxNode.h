#ifndef MUXNODE_H
#define MUXNODE_H

#include <QtNodes/NodeDelegateModel>
#include <QVector>
#include <memory>
#include "GenericQDevIoConnector.h"

/**
 * @brief Multiplexes typed inputs into a mixed QDevIO stream.
 *
 * Input: N ports, one per channel type (Audio, Sensor, etc.)
 * Output: {"QDevIO", "IO"} (mixed stream)
 *
 * Combines typed inputs into a single MixedStreamPayload.
 */
class MuxNode : public QtNodes::NodeDelegateModel
{
    Q_OBJECT

public:
    MuxNode();
    ~MuxNode() override;

    QString caption() const override
    { return QStringLiteral("Mux"); }

    bool captionVisible() const override
    { return false; }

    QString name() const override
    { return QStringLiteral("MuxNode"); }

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
    struct InputPort {
        QString type;
        std::shared_ptr<QtNodes::NodeData> data;
    };

    QVector<InputPort> m_inputs;
    std::shared_ptr<GenericQDevIoConnector> m_output;
};

#endif // MUXNODE_H
