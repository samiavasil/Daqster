#include "PcapModel.h"

#include <QJsonObject>
#include <QTimer>

PcapModel::PcapModel()
{
    m_engine = new PcapEngine(this);
    m_widget = new PcapWidget;

    connect(m_widget, &PcapWidget::startRequested,
            this, &PcapModel::onStartRequested);
    connect(m_widget, &PcapWidget::stopRequested,
            this, &PcapModel::onStopRequested);
    connect(m_widget, &PcapWidget::interfaceChanged,
            this, &PcapModel::onInterfaceChanged);
    connect(m_widget, &PcapWidget::filterChanged,
            this, &PcapModel::onFilterChanged);
    connect(m_widget, &PcapWidget::snaplenChanged,
            this, &PcapModel::onSnaplenChanged);
    connect(m_widget, &PcapWidget::promiscuousChanged,
            this, &PcapModel::onPromiscuousChanged);

    connect(m_engine, &PcapEngine::packetCaptured,
            this, &PcapModel::onPacketCaptured);
    connect(m_engine, &PcapEngine::statusChanged,
            this, &PcapModel::onStatusChanged);
    connect(m_engine, &PcapEngine::errorOccurred,
            this, &PcapModel::onErrorOccurred);
    connect(m_engine, &PcapEngine::statsUpdated,
            this, &PcapModel::onStatsUpdated);

    // Populate interface list from libpcap
#ifdef HAVE_PCAP
    char errbuf[PCAP_ERRBUF_SIZE];
    pcap_if_t *alldevs = nullptr;
    if (pcap_findalldevs(&alldevs, errbuf) == 0) {
        QStringList interfaces;
        for (pcap_if_t *d = alldevs; d; d = d->next) {
            if (d->name)
                interfaces << QString::fromUtf8(d->name);
        }
        m_widget->setInterfaces(interfaces);
        pcap_freealldevs(alldevs);
    } else {
        m_widget->setStatusText(QStringLiteral("pcap_findalldevs failed: %1")
                                .arg(QString::fromUtf8(errbuf)));
    }
#else
    m_widget->setInterfaces(QStringList() << "lo" << "eth0" << "wlan0");
    m_widget->setStatusText(QStringLiteral("libpcap not available (built without HAVE_PCAP)"));
#endif
}

PcapModel::~PcapModel()
{
    // Clean shutdown: stop the capture thread + pcap_close (REQ-SW-PL-047 AC 6).
    m_engine->stop();
    m_widget = nullptr; // owned by the node/view framework
}

QJsonObject PcapModel::save() const
{
    QJsonObject modelJson;
    modelJson["name"] = name();
    modelJson["interface"] = m_widget->interface();
    modelJson["filter"] = m_widget->filter();
    modelJson["snaplen"] = m_widget->snaplen();
    modelJson["promiscuous"] = m_widget->promiscuous();
    return modelJson;
}

void PcapModel::load(QJsonObject const &p)
{
    if (p.contains("interface"))
        m_widget->setInterfaces(QStringList() << p["interface"].toString());
    if (p.contains("filter"))
        m_widget->setFilter(p["filter"].toString());
    if (p.contains("snaplen"))
        m_widget->setSnaplen(p["snaplen"].toInt(65535));
    if (p.contains("promiscuous"))
        m_widget->setPromiscuous(p["promiscuous"].toBool(true));
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

QWidget *PcapModel::embeddedWidget()
{
    return m_widget;
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

void PcapModel::onStartRequested()
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
    m_engine->setInterface(interface);
}

void PcapModel::onFilterChanged(const QString &filter)
{
    m_engine->setFilter(filter);
}

void PcapModel::onSnaplenChanged(int snaplen)
{
    m_engine->setSnaplen(snaplen);
}

void PcapModel::onPromiscuousChanged(bool promiscuous)
{
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
    m_widget->setStatusText(status);
}

void PcapModel::onErrorOccurred(const QString &msg)
{
    m_widget->setStatusText(msg);
    m_widget->setRunning(false);
    m_userStarted = false;
}

void PcapModel::onStatsUpdated(quint64 captured, quint64 dropped, quint64 ifDropped)
{
    m_widget->updateStats(captured, dropped, ifDropped);
}

void PcapModel::setCaptureEnabled(bool enabled)
{
    const bool shouldRun = enabled && m_userStarted;
    if (shouldRun)
        m_engine->start();
    else
        m_engine->stop();
    m_widget->setRunning(shouldRun);
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
    desc.deviceId = m_widget->interface();
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