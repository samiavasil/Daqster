#include "NetworkSinkModel.h"

#include "shared/NetworkFrame.h"

#include <QHostAddress>
#include <QJsonObject>
#include <QTcpSocket>
#include <QUdpSocket>

using QtNodes::NodeDataType;

NetworkSinkModel::NetworkSinkModel()
{
    m_widget = new NetworkSinkWidget();

    connect(m_widget, &NetworkSinkWidget::startRequested,
            this, &NetworkSinkModel::onStartRequested);
    connect(m_widget, &NetworkSinkWidget::stopRequested,
            this, &NetworkSinkModel::onStopRequested);
}

NetworkSinkModel::~NetworkSinkModel()
{
    stopSending();
    m_widget = nullptr;
}

QJsonObject NetworkSinkModel::save() const
{
    QJsonObject modelJson = QtNodes::NodeDelegateModel::save();
    modelJson["protocol"] = m_widget->protocol();
    modelJson["host"] = m_widget->host();
    modelJson["port"] = m_widget->port();
    return modelJson;
}

void NetworkSinkModel::load(QJsonObject const &p)
{
    m_widget->setProtocol(p.value("protocol").toString(QStringLiteral("UDP")));
    m_widget->setHost(p.value("host").toString(QStringLiteral("127.0.0.1")));
    m_widget->setPort(p.value("port").toInt(5000));
}

unsigned int NetworkSinkModel::nPorts(QtNodes::PortType portType) const
{
    return portType == QtNodes::PortType::In ? 1 : 0;
}

QtNodes::NodeDataType NetworkSinkModel::dataType(QtNodes::PortType portType,
                                                 QtNodes::PortIndex portIndex) const
{
    Q_UNUSED(portType);
    Q_UNUSED(portIndex);
    return SampledData().type();
}

std::shared_ptr<QtNodes::NodeData> NetworkSinkModel::outData(QtNodes::PortIndex port)
{
    Q_UNUSED(port);
    return nullptr;
}

void NetworkSinkModel::setInData(std::shared_ptr<QtNodes::NodeData> data,
                                 QtNodes::PortIndex port)
{
    Q_UNUSED(port);

    if (!m_sending)
        return;

    auto sampled = std::dynamic_pointer_cast<SampledData>(data);
    if (!sampled)
        return;

    const QByteArray &buffer = sampled->buffer();
    if (buffer.isEmpty())
        return;

    const int frameBytes = sampled->descriptor().bytesPerFrame();
    const quint32 sampleCount = frameBytes > 0
        ? static_cast<quint32>(buffer.size() / frameBytes)
        : 0;
    const quint32 bytesPerSample = static_cast<quint32>(frameBytes);

    sendFrame(NetworkFrame::encode(buffer, sampleCount, bytesPerSample));
}

QWidget *NetworkSinkModel::embeddedWidget()
{
    return m_widget;
}

// ── Widget slots ────────────────────────────────────────────────────────────

void NetworkSinkModel::onStartRequested()
{
    startSending();
}

void NetworkSinkModel::onStopRequested()
{
    stopSending();
}

// ── Sending helpers ─────────────────────────────────────────────────────────

void NetworkSinkModel::startSending()
{
    if (m_sending)
        return;

    const QString host = m_widget->host();
    const quint16 port = static_cast<quint16>(m_widget->port());
    const bool isUdp = m_widget->protocol() == QLatin1String("UDP");

    if (isUdp) {
        if (!m_udpSocket)
            m_udpSocket = new QUdpSocket(this);
        m_sending = true;
        m_bytesSent = 0;
        updateStatus();
        return;
    }

    // TCP — connect to the remote listener.
    if (!m_tcpSocket)
        m_tcpSocket = new QTcpSocket(this);
    connect(m_tcpSocket, &QTcpSocket::connected,
            this, &NetworkSinkModel::onTcpConnected);
    connect(m_tcpSocket, &QTcpSocket::errorOccurred,
            this, &NetworkSinkModel::onTcpErrorOccurred);

    m_tcpConnected = false;
    m_sending = true;
    m_bytesSent = 0;
    m_tcpSocket->connectToHost(host, port);
    m_widget->setStatus(tr("Connecting to %1:%2...").arg(host).arg(port));
}

void NetworkSinkModel::stopSending()
{
    if (m_udpSocket) {
        m_udpSocket->disconnect(this);
        m_udpSocket->close();
    }
    if (m_tcpSocket) {
        m_tcpSocket->disconnect(this);
        m_tcpSocket->abort();
        m_tcpSocket->deleteLater();
        m_tcpSocket = nullptr;
    }
    m_tcpConnected = false;
    m_sending = false;
    m_widget->setStatus(tr("Idle"));
}

void NetworkSinkModel::sendFrame(const QByteArray &payload)
{
    const QString host = m_widget->host();
    const quint16 port = static_cast<quint16>(m_widget->port());
    const bool isUdp = m_widget->protocol() == QLatin1String("UDP");

    if (isUdp) {
        const QHostAddress addr(host);
        const qint64 written = m_udpSocket->writeDatagram(payload, addr, port);
        if (written < 0) {
            m_widget->setStatus(tr("UDP send error: %1")
                                    .arg(m_udpSocket->errorString()));
            return;
        }
        m_bytesSent += written;
    } else {
        if (!m_tcpConnected) {
            m_widget->setStatus(tr("TCP not connected"));
            return;
        }
        const qint64 written = m_tcpSocket->write(payload);
        if (written < 0) {
            m_widget->setStatus(tr("TCP write error: %1")
                                    .arg(m_tcpSocket->errorString()));
            return;
        }
        m_bytesSent += written;
    }
    updateStatus();
}

void NetworkSinkModel::onTcpConnected()
{
    m_tcpConnected = true;
    updateStatus();
}

void NetworkSinkModel::onTcpErrorOccurred()
{
    if (m_tcpSocket)
        m_widget->setStatus(tr("TCP error: %1").arg(m_tcpSocket->errorString()));
}

void NetworkSinkModel::updateStatus()
{
    if (!m_sending) {
        m_widget->setStatus(tr("Idle"));
        return;
    }
    if (m_widget->protocol() == QLatin1String("TCP") && !m_tcpConnected) {
        m_widget->setStatus(tr("Connecting..."));
        return;
    }
    m_widget->setStatus(tr("Sending — %1 bytes sent").arg(m_bytesSent));
}
