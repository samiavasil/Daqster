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
    m_widget = new NetworkSourceWidget();

    connect(m_widget, &NetworkSourceWidget::startRequested,
            this, &NetworkSourceModel::onStartRequested);
    connect(m_widget, &NetworkSourceWidget::stopRequested,
            this, &NetworkSourceModel::onStopRequested);
}

NetworkSourceModel::~NetworkSourceModel()
{
    stopListening();
    m_widget = nullptr;
}

QJsonObject NetworkSourceModel::save() const
{
    QJsonObject modelJson = QtNodes::NodeDelegateModel::save();
    modelJson["protocol"] = m_widget->protocol();
    modelJson["port"] = m_widget->port();
    modelJson["sampleRate"] = m_widget->sampleRate();
    modelJson["channelCount"] = m_widget->channelCount();
    modelJson["channelType"] = m_widget->channelType();
    return modelJson;
}

void NetworkSourceModel::load(QJsonObject const &p)
{
    m_widget->setProtocol(p.value("protocol").toString(QStringLiteral("UDP")));
    m_widget->setPort(p.value("port").toInt(5000));
    m_widget->setSampleRate(p.value("sampleRate").toDouble(1000.0));
    m_widget->setChannelCount(p.value("channelCount").toInt(2));
    m_widget->setChannelType(p.value("channelType").toString(QStringLiteral("INT16")));
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

QWidget *NetworkSourceModel::embeddedWidget()
{
    return m_widget;
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

// ── Widget slots ────────────────────────────────────────────────────────────

void NetworkSourceModel::onStartRequested()
{
    m_userStarted = true;
    if (m_connectionCount > 0)
        startListening();
    else
        m_widget->setStatus(tr("No output connection"));
}

void NetworkSourceModel::onStopRequested()
{
    m_userStarted = false;
    stopListening();
}

// ── Listening helpers ───────────────────────────────────────────────────────

void NetworkSourceModel::startListening()
{
    if (m_listening)
        return;

    const int port = m_widget->port();
    const bool isUdp = m_widget->protocol() == QLatin1String("UDP");

    if (isUdp) {
        if (!m_udpSocket)
            m_udpSocket = new QUdpSocket(this);
        if (!m_udpSocket->bind(QHostAddress::Any, static_cast<quint16>(port))) {
            m_widget->setStatus(tr("UDP bind failed on port %1: %2")
                                    .arg(port).arg(m_udpSocket->errorString()));
            return;
        }
        connect(m_udpSocket, &QUdpSocket::readyRead,
                this, &NetworkSourceModel::onUdpReadyRead);
    } else {
        if (!m_tcpServer)
            m_tcpServer = new QTcpServer(this);
        if (!m_tcpServer->listen(QHostAddress::Any, static_cast<quint16>(port))) {
            m_widget->setStatus(tr("TCP listen failed on port %1: %2")
                                    .arg(port).arg(m_tcpServer->errorString()));
            return;
        }
        connect(m_tcpServer, &QTcpServer::newConnection,
                this, &NetworkSourceModel::onTcpNewConnection);
    }

    m_listening = true;
    m_bytesReceived = 0;
    updateStatus();
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
    m_tcpBuffer.clear();
    m_listening = false;
    m_widget->setStatus(tr("Idle"));
}

// ── UDP ─────────────────────────────────────────────────────────────────────

void NetworkSourceModel::onUdpReadyRead()
{
    while (m_udpSocket->hasPendingDatagrams()) {
        const QByteArray datagram = m_udpSocket->receiveDatagram().data();
        NetworkFrame::Header hdr;
        QByteArray payload;
        if (NetworkFrame::decode(datagram, hdr, payload)) {
            m_bytesReceived += datagram.size();
            handleFrame(payload);
        }
    }
}

// ── TCP ─────────────────────────────────────────────────────────────────────

void NetworkSourceModel::onTcpNewConnection()
{
    if (m_tcpSocket) {
        // Only one client at a time for v1 — reject extras.
        QTcpSocket *extra = m_tcpServer->nextPendingConnection();
        extra->disconnectFromHost();
        extra->deleteLater();
        return;
    }

    m_tcpSocket = m_tcpServer->nextPendingConnection();
    connect(m_tcpSocket, &QTcpSocket::readyRead,
            this, &NetworkSourceModel::onTcpReadyRead);
    connect(m_tcpSocket, &QTcpSocket::disconnected,
            this, &NetworkSourceModel::onTcpDisconnected);
}

void NetworkSourceModel::onTcpReadyRead()
{
    m_tcpBuffer.append(m_tcpSocket->readAll());

    // Parse complete frames from the stream. Frame length =
    // HeaderSize + sampleCount * bytesPerSample (derived from the header).
    while (m_tcpBuffer.size() >= NetworkFrame::HeaderSize) {
        NetworkFrame::Header hdr;
        QByteArray payload;
        if (!NetworkFrame::decode(m_tcpBuffer.left(NetworkFrame::HeaderSize),
                                  hdr, payload)) {
            // Bad magic — drop the first byte and resync.
            m_tcpBuffer.remove(0, 1);
            continue;
        }

        const qint64 frameLen = NetworkFrame::HeaderSize
            + static_cast<qint64>(hdr.sampleCount) * hdr.bytesPerSample;
        // Sanity bound: a single frame > 256 MB is not a real DAQ stream —
        // drop the connection instead of buffering unbounded garbage.
        if (frameLen > 256 * 1024 * 1024) {
            m_tcpBuffer.clear();
            if (m_tcpSocket) {
                m_tcpSocket->disconnectFromHost();
                m_tcpSocket->deleteLater();
                m_tcpSocket = nullptr;
            }
            updateStatus();
            return;
        }
        if (m_tcpBuffer.size() < frameLen)
            break; // incomplete frame — wait for more data

        const QByteArray frame = m_tcpBuffer.left(static_cast<int>(frameLen));
        m_tcpBuffer.remove(0, static_cast<int>(frameLen));
        m_bytesReceived += frame.size();
        handleFrame(frame.mid(NetworkFrame::HeaderSize));
    }
}

void NetworkSourceModel::onTcpDisconnected()
{
    if (m_tcpSocket) {
        m_tcpSocket->deleteLater();
        m_tcpSocket = nullptr;
    }
    m_tcpBuffer.clear();
    updateStatus();
}

// ── Frame handling ──────────────────────────────────────────────────────────

void NetworkSourceModel::handleFrame(const QByteArray &payload)
{
    if (payload.isEmpty())
        return;

    m_output = std::make_shared<SampledData>(payload, buildDescriptor());
    emit dataUpdated(0);
    updateStatus();
}

SampledStreamDescriptor NetworkSourceModel::buildDescriptor() const
{
    SampledStreamDescriptor desc;
    desc.sampleRate = m_widget->sampleRate();
    desc.domain = QStringLiteral("network");
    desc.sourceName = QStringLiteral("NetworkSource");
    desc.endianness = SampleEndian::LittleEndian;

    const int count = m_widget->channelCount();
    const SampleType type = sampleTypeFromName(m_widget->channelType());
    desc.channels.reserve(count);
    for (int i = 0; i < count; ++i) {
        StreamChannelDescriptor ch;
        ch.name = QStringLiteral("Ch%1").arg(i);
        ch.sampleType = type;
        desc.channels.append(ch);
    }
    return desc;
}

void NetworkSourceModel::updateStatus()
{
    if (m_listening)
        m_widget->setStatus(tr("Listening — %1 bytes received")
                                .arg(m_bytesReceived));
}
