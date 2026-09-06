#ifndef NETWORKSINKMODEL_H
#define NETWORKSINKMODEL_H

#include "NodeDataTypes/SampledData.h"
#include "NetworkSinkWidget.h"

#include <QtNodes/NodeDelegateModel>

#include <memory>

class QTcpSocket;
class QUdpSocket;

/**
 * @brief Network Sink node model (REQ-SW-PL-044).
 *
 * Thin NodeDelegateModel controller: 1 input port (SampledData "sample").
 * On Start it opens a UDP socket / TCP connection to the configured host:port;
 * each incoming SampledData is serialized into a length-prefixed frame (magic
 * "MSSD") and sent. Status shows bytes sent.
 */
class NetworkSinkModel : public QtNodes::NodeDelegateModel
{
    Q_OBJECT

public:
    NetworkSinkModel();
    ~NetworkSinkModel() override;

    QString caption() const override
    { return QStringLiteral("Network Sink"); }

    bool captionVisible() const override
    { return false; }

    QString name() const override
    { return QStringLiteral("NetworkSink"); }

    QJsonObject save() const override;
    void load(QJsonObject const &p) override;

    unsigned int nPorts(QtNodes::PortType portType) const override;

    QtNodes::NodeDataType dataType(QtNodes::PortType portType,
                                   QtNodes::PortIndex portIndex) const override;

    std::shared_ptr<QtNodes::NodeData> outData(QtNodes::PortIndex port) override;

    void setInData(std::shared_ptr<QtNodes::NodeData> data,
                   QtNodes::PortIndex port) override;

    QWidget *embeddedWidget() override;

private slots:
    void onStartRequested();
    void onStopRequested();
    void onTcpConnected();
    void onTcpErrorOccurred();

private:
    void startSending();
    void stopSending();
    void sendFrame(const QByteArray &payload);
    void updateStatus();

    NetworkSinkWidget *m_widget = nullptr;
    QUdpSocket *m_udpSocket = nullptr;
    QTcpSocket *m_tcpSocket = nullptr;
    bool m_sending = false;
    bool m_tcpConnected = false;
    qint64 m_bytesSent = 0;
};

#endif // NETWORKSINKMODEL_H
