#include "PcapModel.h"

#include <QJsonObject>
#include <QTimer>

#ifdef HAVE_PCAP
#include <pcap/pcap.h>
#endif

PcapModel::PcapModel()
{
    m_engine = new PcapEngine(this);

    connect(m_engine, &PcapEngine::packetCaptured,
            this, &PcapModel::onPacketCaptured);
    connect(m_engine, &PcapEngine::statusChanged,
            this, &PcapModel::onStatusChanged);
    connect(m_engine, &PcapEngine::errorOccurred,
            this, &PcapModel::onErrorOccurred);
    connect(m_engine, &PcapEngine::statsUpdated,
            this, &PcapModel::onStatsUpdated);

    // Populate interface list from libpcap - this is now done by the GUI widget
    // when it connects to the engine via setEngine()
}

PcapModel::~PcapModel()
{
    // Single shutdown path: stop() joins the capture thread + pcap_close
    // (REQ-SW-PL-047 AC 6, REQ-SW-PL-050).
    stop();
}

void PcapModel::stop()
{
    // Idempotent: the engine's stop() is safe to call when not capturing.
    if (m_engine)
        m_engine->stop();
    m_userStarted = false;
}

QJsonObject PcapModel::save() const
{
    QJsonObject modelJson;
    modelJson["name"] = name();
    // Interface/filter/snaplen/promiscuous are saved by GUI widget
    return modelJson;
}

void PcapModel::load(QJsonObject const &p)
{
    // Config is applied by GUI widget via NodeWidgetFactory
    Q_UNUSED(p);
}

unsigned int PcapModel::nPorts(QtNodes::PortType portType) const
{
    return portType == QtNodes::PortType::Out ? 1u : 0u;
}

QtNodes::NodeDataType PcapModel::dataType(QtNodes::PortType portType,
                                          QtNodes::PortIndex portIndex) const
{
    Q_UNUSED(portType);
    Q_UNUSED(portIndex);
    return SampledData().type();
}

std::shared_ptr<QtNodes::NodeData> PcapModel::outData(QtNodes::PortIndex port)
{
    Q_UNUSED(port);
    return m_lastData;
}

void PcapModel::setInData(std::shared_ptr<QtNodes::NodeData> data,
                          QtNodes::PortIndex port)
{
    Q_UNUSED(data);
    Q_UNUSED(port);
    Q_ASSERT(0);
}

void PcapModel::outputConnectionCreated(QtNodes::ConnectionId const &conId)
{
    Q_UNUSED(conId);
    ++m_connectionCount;
    setCaptureEnabled(m_connectionCount > 0);
}

void PcapModel::outputConnectionDeleted(QtNodes::ConnectionId const &conId)
{
    Q_UNUSED(conId);
    if (m_connectionCount > 0)
        --m_connectionCount;
    setCaptureEnabled(m_connectionCount > 0);
}

// Public slots called by GUI widget via NodeWidgetFactory

void PcapModel::onStartRequested()
{
    m_userStarted = true;
    setCaptureEnabled(true);
}

void PcapModel::start()
{
    m_userStarted = true;
    setCaptureEnabled(true);
}

void PcapModel::onStopRequested()
{
    m_userStarted = false;
    setCaptureEnabled(false);
}

void PcapModel::onInterfaceChanged(const QString &interface)
{
    if (m_engine)
        m_engine->setInterface(interface);
}

void PcapModel::onFilterChanged(const QString &filter)
{
    if (m_engine)
        m_engine->setFilter(filter);
}

void PcapModel::onSnaplenChanged(int snaplen)
{
    if (m_engine)
        m_engine->setSnaplen(snaplen);
}

void PcapModel::onPromiscuousChanged(bool promiscuous)
{
    if (m_engine)
        m_engine->setPromiscuous(promiscuous);
}

void PcapModel::onPacketCaptured(const PcapEngine::Packet &packet)
{
    // Enqueue packet for processing on GUI thread
    enqueuePacket(packet);

    // Process queued packets (limit to avoid blocking GUI too long)
    PcapEngine::Packet pkt;
    int processed = 0;
    while (dequeuePacket(pkt) && processed < 100) {
        m_lastData = buildSampledData(pkt);
        emit dataUpdated(0);
        ++processed;
    }

    // If more packets queued, schedule another processing round
    if (!m_packetQueue.empty()) {
        QTimer::singleShot(0, this, [this]() {
            PcapEngine::Packet p;
            int processed = 0;
            while (dequeuePacket(p) && processed < 100) {
                m_lastData = buildSampledData(p);
                emit dataUpdated(0);
                ++processed;
            }
        });
    }
}

void PcapModel::onStatusChanged(const QString &status)
{
    // GUI widget (if present) will be updated via its own connection to engine
    Q_UNUSED(status);
}

void PcapModel::onErrorOccurred(const QString &msg)
{
    // GUI widget (if present) will be updated via its own connection to engine
    Q_UNUSED(msg);
    m_userStarted = false;
}

void PcapModel::onStatsUpdated(quint64 captured, quint64 dropped, quint64 ifDropped)
{
    // GUI widget (if present) will be updated via its own connection to engine
    Q_UNUSED(captured);
    Q_UNUSED(dropped);
    Q_UNUSED(ifDropped);
}

void PcapModel::setCaptureEnabled(bool enabled)
{
    const bool shouldRun = enabled && m_userStarted;
    if (shouldRun)
        m_engine->start();
    else
        m_engine->stop();
    // GUI widget (if present) will update its own running state
}

std::shared_ptr<SampledData> PcapModel::buildSampledData(const PcapEngine::Packet &pkt) const
{
    SampledStreamDescriptor desc;
    desc.sampleRate = 0.0; // event-driven
    desc.channels = {
        {QStringLiteral("packet"), SampleType::UINT8}, // raw bytes
    };
    desc.endianness = SampleEndian::LittleEndian;
    desc.unit = QStringLiteral("raw");
    desc.domain = QStringLiteral("pcap");
    desc.deviceId = m_engine ? m_engine->currentInterface() : QStringLiteral("unknown");
    desc.sourceName = QStringLiteral("pcap capture");

    // Store metadata in the buffer as a prefix or use the descriptor's meta fields
    // For now, we store the raw packet bytes. Metadata (timestamp, caplen, len)
    // could be added as additional channels or via a custom mechanism.
    // The simplest approach: just the packet payload as BYTES.

    QByteArray buffer = pkt.data;

    return std::make_shared<SampledData>(buffer, desc);
}

void PcapModel::enqueuePacket(const PcapEngine::Packet &pkt)
{
    std::lock_guard<std::mutex> lock(m_queueMutex);
    m_packetQueue.push(pkt);
}

bool PcapModel::dequeuePacket(PcapEngine::Packet &pkt)
{
    std::lock_guard<std::mutex> lock(m_queueMutex);
    if (m_packetQueue.empty())
        return false;
    pkt = m_packetQueue.front();
    m_packetQueue.pop();
    return true;
}