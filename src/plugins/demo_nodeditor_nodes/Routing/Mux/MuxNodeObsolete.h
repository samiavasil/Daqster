#ifndef MUXNODEOBSOLETE_H
#define MUXNODEOBSOLETE_H

#include <QtNodes/NodeDelegateModel>
#include <QVector>
#include <memory>
#include "GenericQDevIoConnectorObsolete.h"

/**
 * @brief Multiplexes typed inputs into a mixed QDevIO stream.
 *
 * Input: N ports, one per channel type (Audio, Sensor, etc.)
 * Output: {"QDevIO", "IO"} (mixed stream)
 *
 * Combines typed inputs into a single MixedStreamPayload.
 *
 * @note Renamed to *_obsolete (REQ-SW-PL-023 §7) — implementation unchanged.
 *       Kept working until the QDevIO display world is deleted at the very end.
 *       Registered under its new name AND aliased under the old "MuxNode" key
 *       so old saved graphs still load.
 */
class MuxNodeObsolete : public QtNodes::NodeDelegateModel
{
    Q_OBJECT

public:
    MuxNodeObsolete();
    ~MuxNodeObsolete() override;

    QString caption() const override
    { return QStringLiteral("Mux (obsolete)"); }

    bool captionVisible() const override
    { return false; }

    QString name() const override
    { return QStringLiteral("MuxNodeObsolete"); }

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
    std::shared_ptr<GenericQDevIoConnectorObsolete> m_output;
};

#endif // MUXNODEOBSOLETE_H
