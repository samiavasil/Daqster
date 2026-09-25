#ifndef NETWORKSINKMODEL_H
#define NETWORKSINKMODEL_H

#include "NodeDataTypes/SampledData.h"
#include "shared/IStoppable.h"
#include "shared/IStartable.h"

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
 *
 * The GUI widget is provided by the GUI plugin via NodeWidgetFactory.
 * Core model has no QtWidgets dependency — returns nullptr from embeddedWidget().
 */
class NetworkSinkModel : public QtNodes::NodeDelegateModel, public Daqster::IStoppable, public Daqster::IStartable
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

    QWidget *embeddedWidget() override { return nullptr; }

    /// Stop sending and close the socket. Idempotent — safe to call multiple
    /// times (REQ-SW-PL-050).
    void stop() override;

    /// Start sending programmatically (runtime autoStart, REQ-SW-PL-048).
    void start() override;

signals:
    void statusChanged(const QString &status);

public slots:
    void onStartRequested();
    void onStopRequested();
    void onTcpConnected();
    void onTcpErrorOccurred();

private:
    void startSending();
    void stopSending();
    void sendFrame(const QByteArray &payload);
    void updateStatus(const QString &status);

    QUdpSocket *m_udpSocket = nullptr;
    QTcpSocket *m_tcpSocket = nullptr;
    bool m_sending = false;
    bool m_tcpConnected = false;
    qint64 m_bytesSent = 0;

    // Config from GUI widget
    QString m_protocol = "UDP";
    QString m_host = "127.0.0.1";
    int m_port = 5000;
};

#endif // NETWORKSINKMODEL_H