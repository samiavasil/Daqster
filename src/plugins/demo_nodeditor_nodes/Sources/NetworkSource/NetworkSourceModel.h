#ifndef NETWORKSOURCEMODEL_H
#define NETWORKSOURCEMODEL_H

#include "NodeDataTypes/SampledData.h"
#include "NetworkSourceWidget.h"

#include <QtNodes/NodeDelegateModel>

#include <QByteArray>
#include <QHostAddress>

#include <memory>

class QTcpServer;
class QTcpSocket;
class QUdpSocket;

/**
 * @brief Network Source node model (REQ-SW-PL-044).
 *
 * Thin NodeDelegateModel controller: 1 output port (SampledData "sample").
 * Listens on a port (UDP via QUdpSocket, TCP via QTcpServer), receives
 * length-prefixed frames (magic "MSSD"), reconstructs a SampledData using the
 * UI-configured descriptor (sampleRate, channels) and emits dataUpdated(0).
 *
 * Connection-count gating (model of SystemMonitorModel/FilePlaybackModel): the
 * listener runs only while the user pressed Start AND at least one output
 * connection exists; removing the last connection auto-stops the listener.
 */
class NetworkSourceModel : public QtNodes::NodeDelegateModel
{
    Q_OBJECT

public:
    NetworkSourceModel();
    ~NetworkSourceModel() override;

    QString caption() const override
    { return QStringLiteral("Network Source"); }

    bool captionVisible() const override
    { return false; }

    QString name() const override
    { return QStringLiteral("NetworkSource"); }

    QJsonObject save() const override;
    void load(QJsonObject const &p) override;

    unsigned int nPorts(QtNodes::PortType portType) const override;

    QtNodes::NodeDataType dataType(QtNodes::PortType portType,
                                   QtNodes::PortIndex portIndex) const override;

    std::shared_ptr<QtNodes::NodeData> outData(QtNodes::PortIndex port) override;

    void setInData(std::shared_ptr<QtNodes::NodeData> data,
                   QtNodes::PortIndex port) override;

    QWidget *embeddedWidget() override;

    void outputConnectionCreated(QtNodes::ConnectionId const &) override;
    void outputConnectionDeleted(QtNodes::ConnectionId const &) override;

private slots:
    void onStartRequested();
    void onStopRequested();
    void onUdpReadyRead();
    void onTcpNewConnection();
    void onTcpReadyRead();
    void onTcpDisconnected();

private:
    void startListening();
    void stopListening();
    void handleFrame(const QByteArray &payload);
    void updateStatus();
    SampledStreamDescriptor buildDescriptor() const;

    NetworkSourceWidget *m_widget = nullptr;
    QUdpSocket *m_udpSocket = nullptr;
    QTcpServer *m_tcpServer = nullptr;
    QTcpSocket *m_tcpSocket = nullptr;
    QByteArray m_tcpBuffer;
    std::shared_ptr<SampledData> m_output;
    int m_connectionCount = 0;
    bool m_userStarted = false;
    bool m_listening = false;
    qint64 m_bytesReceived = 0;
};

#endif // NETWORKSOURCEMODEL_H
