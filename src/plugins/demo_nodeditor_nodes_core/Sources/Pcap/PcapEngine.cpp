#include "PcapEngine.h"

#include <QThread>
#include <QTimer>

#ifdef HAVE_PCAP
#include <pcap/pcap.h>
#else
// Forward declarations for when libpcap is not available
struct pcap_t;
struct pcap_pkthdr;
typedef int bpf_int32;
typedef unsigned int bpf_u_int32;
struct bpf_program {
    int bf_len;
    struct bpf_insn *bf_insns;
};
#endif

PcapEngine::PcapEngine(QObject *parent)
    : QObject(parent)
{
}

PcapEngine::~PcapEngine()
{
    stop();
}

void PcapEngine::setInterface(const QString &interface)
{
    QMutexLocker locker(&m_mutex);
    if (!m_running)
        m_interface = interface;
}

void PcapEngine::setFilter(const QString &filter)
{
    QMutexLocker locker(&m_mutex);
    if (!m_running)
        m_filter = filter;
}

void PcapEngine::setSnaplen(int snaplen)
{
    QMutexLocker locker(&m_mutex);
    if (!m_running)
        m_snaplen = qBound(68, snaplen, 262144);
}

void PcapEngine::setPromiscuous(bool promiscuous)
{
    QMutexLocker locker(&m_mutex);
    if (!m_running)
        m_promiscuous = promiscuous;
}

bool PcapEngine::open()
{
#ifdef HAVE_PCAP
    if (m_handle)
        return true;

    if (m_interface.isEmpty()) {
        emit errorOccurred(QStringLiteral("No interface selected"));
        return false;
    }

    char errbuf[PCAP_ERRBUF_SIZE];
    m_handle = pcap_open_live(m_interface.toUtf8().constData(), m_snaplen,
                              m_promiscuous ? 1 : 0, 1000, errbuf);
    if (!m_handle) {
        emit errorOccurred(QStringLiteral("pcap_open_live failed: %1")
                           .arg(QString::fromUtf8(errbuf)));
        return false;
    }

    if (!m_filter.isEmpty()) {
        struct bpf_program fp;
        bpf_u_int32 net = 0;
        if (pcap_compile(m_handle, &fp, m_filter.toUtf8().constData(), 1, net) == -1) {
            emit errorOccurred(QStringLiteral("pcap_compile failed: %1")
                               .arg(QString::fromUtf8(pcap_geterr(m_handle))));
            pcap_close(m_handle);
            m_handle = nullptr;
            return false;
        }
        if (pcap_setfilter(m_handle, &fp) == -1) {
            emit errorOccurred(QStringLiteral("pcap_setfilter failed: %1")
                               .arg(QString::fromUtf8(pcap_geterr(m_handle))));
            pcap_freecode(&fp);
            pcap_close(m_handle);
            m_handle = nullptr;
            return false;
        }
        pcap_freecode(&fp);
    }

    m_status = QStringLiteral("Opened %1 (snaplen=%2, promisc=%3)")
                   .arg(m_interface).arg(m_snaplen).arg(m_promiscuous ? "yes" : "no");
    emit statusChanged(m_status);
    return true;
#else
    Q_UNUSED(m_interface);
    Q_UNUSED(m_filter);
    Q_UNUSED(m_snaplen);
    Q_UNUSED(m_promiscuous);
    emit errorOccurred(QStringLiteral("libpcap not available (built without HAVE_PCAP)"));
    return false;
#endif
}

void PcapEngine::close()
{
#ifdef HAVE_PCAP
    if (m_handle) {
        pcap_close(m_handle);
        m_handle = nullptr;
    }
#else
    m_handle = nullptr;
#endif
    m_status = QStringLiteral("Closed");
    emit statusChanged(m_status);
}

void PcapEngine::start()
{
    if (m_running)
        return;

    if (!open())
        return;

    m_running = true;
    m_packetsCaptured = 0;
    m_packetsDropped = 0;
    m_packetsIfDropped = 0;

    m_thread = new QThread(this);
    connect(m_thread, &QThread::started, this, &PcapEngine::runCapture);
    connect(m_thread, &QThread::finished, m_thread, &QThread::deleteLater);
    m_thread->start();

    m_status = QStringLiteral("Capturing on %1...").arg(m_interface);
    emit statusChanged(m_status);
}

void PcapEngine::stop()
{
    if (!m_running)
        return;

    m_running = false;

#ifdef HAVE_PCAP
    if (m_handle) {
        pcap_breakloop(m_handle);
    }
#endif

    if (m_thread) {
        m_thread->quit();
        if (!m_thread->wait(3000)) {
            m_thread->terminate();
            m_thread->wait(1000);
        }
        m_thread = nullptr;
    }

    close();

    m_status = QStringLiteral("Stopped (%1 packets)").arg(m_packetsCaptured);
    emit statusChanged(m_status);
}

void PcapEngine::runCapture()
{
#ifdef HAVE_PCAP
    if (!m_handle)
        return;

    const u_char *packetData;
    struct pcap_pkthdr *header = nullptr;

    // Stats update timer
    QTimer statsTimer;
    statsTimer.setInterval(1000);
    connect(&statsTimer, &QTimer::timeout, this, &PcapEngine::updateStats);
    statsTimer.start();

    while (m_running) {
        int result = pcap_next_ex(m_handle, &header, &packetData);
        if (result == 1) {
            // Packet captured
            Packet pkt;
            pkt.data = QByteArray(reinterpret_cast<const char *>(packetData), header->caplen);
            pkt.timestampUs = static_cast<qint64>(header->ts.tv_sec) * 1000000LL
                            + static_cast<qint64>(header->ts.tv_usec);
            pkt.caplen = header->caplen;
            pkt.len = header->len;

            ++m_packetsCaptured;
            emit packetCaptured(pkt);
        } else if (result == 0) {
            // Timeout (pcap_next_ex with 1000ms timeout from pcap_open_live)
            continue;
        } else if (result == PCAP_ERROR_BREAK) {
            // pcap_breakloop() called
            break;
        } else {
            // Error
            emit errorOccurred(QStringLiteral("pcap_next_ex error: %1")
                               .arg(QString::fromUtf8(pcap_geterr(m_handle))));
            break;
        }
    }

    statsTimer.stop();
    updateStats();
#else
    Q_UNUSED(m_handle);
    emit errorOccurred(QStringLiteral("libpcap not available (built without HAVE_PCAP)"));
#endif
}

void PcapEngine::updateStats()
{
#ifdef HAVE_PCAP
    if (!m_handle)
        return;

    struct pcap_stat stats;
    if (pcap_stats(m_handle, &stats) == 0) {
        m_packetsDropped = stats.ps_drop;
        m_packetsIfDropped = stats.ps_ifdrop;
        emit statsUpdated(m_packetsCaptured, m_packetsDropped, m_packetsIfDropped);
    }
#else
    Q_UNUSED(m_handle);
#endif
}