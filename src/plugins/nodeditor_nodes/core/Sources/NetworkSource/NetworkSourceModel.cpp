#include "NetworkSourceModel.h"

#include "shared/NetworkFrame.h"

#include <QHostAddress>
#include <QJsonObject>
#include <QNetworkDatagram>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUdpSocket>

using QtNodes::NodeDataType;

namespace {

SampleType sampleTypeFromName(const QString &name)
{
    if (name == QLatin1String("INT16"))
        return SampleType::INT16;
    if (name == QLatin1String("FLOAT32"))
        return SampleType::FLOAT32;
    return SampleType::INT16;
}

} // namespace

NetworkSourceModel::NetworkSourceModel()
{
    // No widget in core model - widget is created by GUI plugin via NodeWidgetFactory
}

NetworkSourceModel::~NetworkSourceModel()
{
    // Single shutdown path: stop() closes the listener (REQ-SW-PL-050).
    stop();
}

void NetworkSourceModel::stop()
{
    // Idempotent: stopListening() on an already-stopped listener is a no-op.
    stopListening();
    m_userStarted = false;
}

/// Start listening programmatically (runtime autoStart, REQ-SW-PL-048).
/// Delegates to the same logic as onStartRequested().
void NetworkSourceModel::start()
{
    m_userStarted = true;
    if (m_connectionCount > 0)
        startListening();
}

QJsonObject NetworkSourceModel::save() const
{
    QJsonObject modelJson = QtNodes::NodeDelegateModel::save();
    modelJson["protocol"] = m_protocol;
    modelJson["port"] = m_port;
    modelJson["sampleRate"] = m_sampleRate;
    modelJson["channelCount"] = m_channelCount;
    modelJson["channelType"] = m_channelType;
    return modelJson;
}

void NetworkSourceModel::load(QJsonObject const &p)
{
    m_protocol = p.value("protocol").toString(QStringLiteral("UDP"));
    m_port = p.value("port").toInt(5000);
    m_sampleRate = p.value("sampleRate").toDouble(1000.0);
    m_channelCount = p.value("channelCount").toInt(2);
    m_channelType = p.value("channelType").toString(QStringLiteral("INT16"));
}

unsigned int NetworkSourceModel::nPorts(QtNodes::PortType portType) const
{
    return portType == QtNodes::PortType::Out ? 1 : 0;
}

QtNodes::NodeDataType NetworkSourceModel::dataType(QtNodes::PortType portType,
                                                   QtNodes::PortIndex portIndex) const
{
    Q_UNUSED(portType);
    Q_UNUSED(portIndex);
    return SampledData().type();
}

std::shared_ptr<QtNodes::NodeData> NetworkSourceModel::outData(QtNodes::PortIndex port)
{
    Q_UNUSED(port);
    return m_output;
}

void NetworkSourceModel::setInData(std::shared_ptr<QtNodes::NodeData> data,
                                   QtNodes::PortIndex port)
{
    Q_UNUSED(data);
    Q_UNUSED(port);
    Q_ASSERT(0);
}

// ── Connection-count gating (model of SystemMonitorModel) ───────────────────

void NetworkSourceModel::outputConnectionCreated(QtNodes::ConnectionId const &conId)
{
    Q_UNUSED(conId);
    ++m_connectionCount;
    if (m_userStarted && m_connectionCount > 0)
        startListening();
}

void NetworkSourceModel::outputConnectionDeleted(QtNodes::ConnectionId const &conId)
{
    Q_UNUSED(conId);
    if (m_connectionCount > 0)
        --m_connectionCount;
    if (m_connectionCount == 0)
        stopListening();
}

// ── Public slots (called by GUI widget via NodeWidgetFactory) ──────────────

void NetworkSourceModel::onStartRequested()
{
    m_userStarted = true;
    if (m_connectionCount > 0)
        startListening();
    else
        emit statusChanged(tr("No output connection"));
}

void NetworkSourceModel::onStopRequested()
{
    m_userStarted = false;
    stopListening();
}

void NetworkSourceModel::onProtocolChanged(const QString &protocol)
{
    m_protocol = protocol;
}

void NetworkSourceModel::onHostChanged(const QString &host)
{
    m_host = host;
}

void NetworkSourceModel::onPortChanged(int port)
{
    m_port = port;
}

void NetworkSourceModel::onSampleRateChanged(double rate)
{
    m_sampleRate = rate;
}

void NetworkSourceModel::onChannelCountChanged(int count)
{
    m_channelCount = count;
}

void NetworkSourceModel::onChannelTypeChanged(const QString &type)
{
    m_channelType = type;
}

// ── Listening helpers ───────────────────────────────────────────────────────

void NetworkSourceModel::startListening()
{
    if (m_listening)
        return;

    if (m_protocol == QLatin1String("UDP")) {
        if (!m_udpSocket)
            m_udpSocket = new QUdpSocket(this);
        if (!m_udpSocket->bind(QHostAddress::Any, static_cast<quint16>(m_port))) {
            emit statusChanged(tr("UDP bind failed on port %1: %2")
                                .arg(m_port).arg(m_udpSocket->errorString()));
            return;
        }
        connect(m_udpSocket, &QUdpSocket::readyRead,
                this, &NetworkSourceModel::onUdpReadyRead);
    } else {
        if (!m_tcpServer)
            m_tcpServer = new QTcpServer(this);
        if (!m_tcpServer->listen(QHostAddress::Any, static_cast<quint16>(m_port))) {
            emit statusChanged(tr("TCP listen failed on port %1: %2")
                                .arg(m_port).arg(m_tcpServer->errorString()));
            return;
        }
        connect(m_tcpServer, &QTcpServer::newConnection,
                this, &NetworkSourceModel::onTcpNewConnection);
    }

    m_listening = true;
    m_bytesReceived = 0;
    updateStatus(tr("Listening on port %1 (%2)").arg(m_port).arg(m_protocol));
}

void NetworkSourceModel::stopListening()
{
    if (m_udpSocket) {
        m_udpSocket->disconnect(this);
        m_udpSocket->close();
    }
    if (m_tcpServer) {
        m_tcpServer->disconnect(this);
        m_tcpServer->close();
    }
    if (m_tcpSocket) {
        m_tcpSocket->disconnect(this);
        m_tcpSocket->close();
        m_tcpSocket->deleteLater();
        m_tcpSocket = nullptr;
    }
    m_listening = false;
    updateStatus(tr("Stopped"));
}

void NetworkSourceModel::onUdpReadyRead()
{
    while (m_udpSocket && m_udpSocket->hasPendingDatagrams()) {
        QNetworkDatagram dgram = m_udpSocket->receiveDatagram();
        handleFrame(dgram.data());
    }
}

void NetworkSourceModel::onTcpNewConnection()
{
    if (m_tcpSocket) {
        // Only accept one TCP client at a time
        m_tcpSocket->disconnect(this);
        m_tcpSocket->close();
        m_tcpSocket->deleteLater();
    }
    m_tcpSocket = m_tcpServer->nextPendingConnection();
    connect(m_tcpSocket, &QTcpSocket::readyRead,
            this, &NetworkSourceModel::onTcpReadyRead);
    connect(m_tcpSocket, &QTcpSocket::disconnected,
            this, &NetworkSourceModel::onTcpDisconnected);
    updateStatus(tr("TCP client connected"));
}

void NetworkSourceModel::onTcpReadyRead()
{
    if (!m_tcpSocket)
        return;

    m_tcpBuffer.append(m_tcpSocket->readAll());

    while (m_tcpBuffer.size() >= NetworkFrame::LengthPrefixSize) {
        const quint32 frameSize = qFromLittleEndian<quint32>(
            reinterpret_cast<const uchar *>(m_tcpBuffer.constData()));

        if (frameSize < static_cast<quint32>(NetworkFrame::HeaderSize)
            || frameSize > static_cast<quint32>(NetworkFrame::MaxFrameSize)) {
            m_tcpBuffer.clear();
            updateStatus(tr("Invalid frame size %1").arg(frameSize));
            return;
        }

        const int total = NetworkFrame::LengthPrefixSize + static_cast<int>(frameSize);
        if (m_tcpBuffer.size() < total)
            break; // frame still incomplete — wait for the rest of the stream

        handleFrame(m_tcpBuffer.mid(NetworkFrame::LengthPrefixSize,
                                    static_cast<int>(frameSize)));
        m_tcpBuffer.remove(0, total);
    }
}

void NetworkSourceModel::onTcpDisconnected()
{
    if (m_tcpSocket) {
        m_tcpSocket->disconnect(this);
        m_tcpSocket->deleteLater();
        m_tcpSocket = nullptr;
    }
    updateStatus(tr("TCP client disconnected"));
}

void NetworkSourceModel::handleFrame(const QByteArray &payload)
{
    NetworkFrame::Header hdr;
    QByteArray raw;
    if (!NetworkFrame::decode(payload, hdr, raw)) {
        updateStatus(tr("Invalid frame (magic, version or length)"));
        return;
    }

    // v2 carries the real descriptor, so the receiving side never has to guess
    // how to interpret the bytes it was handed.
    SampledStreamDescriptor desc = NetworkFrame::descriptorFor(hdr);
    desc.deviceId = m_host.isEmpty() ? QStringLiteral("local") : m_host;
    desc.sourceName = QStringLiteral("Network Source");

    m_output = std::make_shared<SampledData>(raw, desc);
    m_bytesReceived += raw.size();
    emit dataUpdated(0);

    // The widget's descriptor fields are advisory now — they cannot change how
    // the payload is read. Warn once when they disagree with the wire, so a
    // stale hand-typed rate is visible instead of silently ignored.
    const bool uiDiffers = hdr.sampleRate != m_sampleRate
        || static_cast<int>(hdr.channelCount) != m_channelCount
        || sampleTypeName(hdr.sampleType).compare(m_channelType, Qt::CaseInsensitive) != 0;

    if (uiDiffers && !m_warnedDescriptorMismatch) {
        m_warnedDescriptorMismatch = true;
        updateStatus(tr("Wire %1 Hz / %2 ch / %3 overrides UI %4 Hz / %5 ch / %6")
                         .arg(hdr.sampleRate)
                         .arg(hdr.channelCount)
                         .arg(sampleTypeName(hdr.sampleType))
                         .arg(m_sampleRate)
                         .arg(m_channelCount)
                         .arg(m_channelType));
        return;
    }

    updateStatus(tr("Received %1 bytes").arg(m_bytesReceived));
}

void NetworkSourceModel::updateStatus(const QString &status)
{
    emit statusChanged(status);
}