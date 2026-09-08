#include "NetworkSinkModel.h"

#include "shared/NetworkFrame.h"

#include <QHostAddress>
#include <QJsonObject>
#include <QTcpSocket>
#include <QUdpSocket>

using QtNodes::NodeDataType;

NetworkSinkModel::NetworkSinkModel()
{
    // No widget in core model - widget is created by GUI plugin via NodeWidgetFactory
}

NetworkSinkModel::~NetworkSinkModel()
{
    // Single shutdown path: stop() closes the socket (REQ-SW-PL-050).
    stop();
}

void NetworkSinkModel::stop()
{
    // Idempotent: stopSending() on an already-stopped sink is a no-op.
    stopSending();
}

/// Start sending programmatically (runtime autoStart, REQ-SW-PL-048).
/// Delegates to the same logic as onStartRequested().
void NetworkSinkModel::start()
{
    startSending();
}

QJsonObject NetworkSinkModel::save() const
{
    QJsonObject modelJson = QtNodes::NodeDelegateModel::save();
    modelJson["protocol"] = m_protocol;
    modelJson["host"] = m_host;
    modelJson["port"] = m_port;
    return modelJson;
}

void NetworkSinkModel::load(QJsonObject const &p)
{
    m_protocol = p.value("protocol").toString(QStringLiteral("UDP"));
    m_host = p.value("host").toString(QStringLiteral("127.0.0.1"));
    m_port = p.value("port").toInt(5000);
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

// ── Public slots (called by GUI widget via NodeWidgetFactory) ──────────────

void NetworkSinkModel::onStartRequested()
{
    startSending();
}

void NetworkSinkModel::onStopRequested()
{
    stopSending();
}

void NetworkSinkModel::onTcpConnected()
{
    m_tcpConnected = true;
    updateStatus(tr("TCP connected — %1 bytes sent").arg(m_bytesSent));
}

void NetworkSinkModel::onTcpErrorOccurred()
{
    if (m_tcpSocket)
        updateStatus(tr("TCP error: %1").arg(m_tcpSocket->errorString()));
}

// ── Sending helpers ─────────────────────────────────────────────────────────

void NetworkSinkModel::startSending()
{
    if (m_sending)
        return;

    const quint16 port = static_cast<quint16>(m_port);
    const bool isUdp = m_protocol == QLatin1String("UDP");

    if (isUdp) {
        if (!m_udpSocket)
            m_udpSocket = new QUdpSocket(this);
        m_sending = true;
        m_bytesSent = 0;
        updateStatus(tr("UDP sending to %1:%2").arg(m_host).arg(port));
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
    m_tcpSocket->connectToHost(m_host, port);
    updateStatus(tr("Connecting to %1:%2...").arg(m_host).arg(port));
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
    updateStatus(tr("Idle"));
}

void NetworkSinkModel::sendFrame(const QByteArray &payload)
{
    const quint16 port = static_cast<quint16>(m_port);
    const bool isUdp = m_protocol == QLatin1String("UDP");

    if (isUdp) {
        const QHostAddress addr(m_host);
        const qint64 written = m_udpSocket->writeDatagram(payload, addr, port);
        if (written < 0) {
            updateStatus(tr("UDP send error: %1")
                            .arg(m_udpSocket->errorString()));
            return;
        }
        m_bytesSent += written;
    } else {
        if (!m_tcpConnected) {
            updateStatus(tr("TCP not connected"));
            return;
        }
        const qint64 written = m_tcpSocket->write(payload);
        if (written < 0) {
            updateStatus(tr("TCP write error: %1")
                            .arg(m_tcpSocket->errorString()));
            return;
        }
        m_bytesSent += written;
    }
    updateStatus(tr("Sending — %1 bytes sent").arg(m_bytesSent));
}

void NetworkSinkModel::updateStatus(const QString &status)
{
    emit statusChanged(status);
}