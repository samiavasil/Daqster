#ifndef PCAPENGINE_H
#define PCAPENGINE_H

#include <QObject>
#include <QString>
#include <QVector>
#include <QMutex>
#include <QWaitCondition>

#ifdef HAVE_PCAP
#include <pcap/pcap.h>
#endif

class QThread;

/**
 * @brief libpcap wrapper that captures packets in a worker thread.
 *
 * REQ-SW-PL-047: opens a network interface via libpcap, applies a BPF filter,
 * runs pcap_loop() in a dedicated QThread, and emits packetCaptured() for each
 * packet. All libpcap calls are guarded by #ifdef HAVE_PCAP — without libpcap
 * the class compiles as a no-op (open() returns false, start() emits nothing).
 *
 * The engine owns a QThread; start() begins capture, stop() calls
 * pcap_breakloop() and waits for the thread to finish. The caller (PcapModel)
 * owns the engine and is responsible for calling stop() on destruction.
 */
class PcapEngine : public QObject
{
    Q_OBJECT

public:
    /// One captured packet with metadata.
    struct Packet {
        QByteArray data;           // raw packet bytes (payload)
        qint64 timestampUs = 0;    // microseconds since epoch (pcap_ts.tv_sec * 1e6 + tv_usec)
        quint32 caplen = 0;        // captured length (bytes actually captured)
        quint32 len = 0;           // original packet length on wire
    };

    explicit PcapEngine(QObject *parent = nullptr);
    ~PcapEngine() override;

    /// Set the network interface name (e.g. "eth0", "lo", "wlan0").
    void setInterface(const QString &interface);

    /// Set the BPF filter expression (e.g. "tcp port 80", "udp", "" for none).
    void setFilter(const QString &filter);

    /// Set the snaplen (max bytes to capture per packet, default 65535).
    void setSnaplen(int snaplen);

    /// Set promiscuous mode (default true).
    void setPromiscuous(bool promiscuous);

    /// Open the interface and compile the filter. Returns false on failure
    /// (or when libpcap is not compiled in).
    bool open();

    /// Close the pcap handle. Safe to call multiple times.
    void close();

    /// Start capturing: open() if needed, then launch the worker thread.
    void start();

    /// Stop capturing: pcap_breakloop() + thread join + close().
    void stop();

    /// Human-readable status string.
    QString status() const { return m_status; }

    /// Number of packets captured since start().
    quint64 packetsCaptured() const { return m_packetsCaptured; }

    /// Number of packets dropped by kernel (pcap_stat.ps_drop).
    quint64 packetsDropped() const { return m_packetsDropped; }

    /// Number of packets dropped by interface (pcap_stat.ps_ifdrop).
    quint64 packetsIfDropped() const { return m_packetsIfDropped; }

signals:
    void packetCaptured(const PcapEngine::Packet &packet);
    void statusChanged(const QString &status);
    void errorOccurred(const QString &msg);
    void statsUpdated(quint64 captured, quint64 dropped, quint64 ifDropped);

private:
    void runCapture();
    void updateStats();

#ifdef HAVE_PCAP
    pcap_t *m_handle = nullptr;
#else
    void *m_handle = nullptr;
#endif
    QThread *m_thread = nullptr;
    QString m_interface;
    QString m_filter;
    int m_snaplen = 65535;
    bool m_promiscuous = true;
    bool m_running = false;
    QString m_status;
    quint64 m_packetsCaptured = 0;
    quint64 m_packetsDropped = 0;
    quint64 m_packetsIfDropped = 0;
    mutable QMutex m_mutex;
};

#endif // PCAPENGINE_H