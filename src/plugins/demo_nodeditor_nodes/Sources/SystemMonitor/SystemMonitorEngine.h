#ifndef SYSTEMMONITORENGINE_H
#define SYSTEMMONITORENGINE_H

#include <QObject>
#include <QString>

#include <QtGlobal>

class QTimer;

/**
 * @brief Current system telemetry values (REQ-SW-PL-041).
 *
 * Top-level struct (not nested) so Qt moc can resolve it in the
 * metricsReady() signal / onMetricsReady() slot signature.
 */
struct SystemMonitorMetrics {
    double cpuPercent = 0.0;
    double ramPercent = 0.0;
    double cpuTempC = 0.0;
    double netRxKbps = 0.0;
    double netTxKbps = 0.0;
};

/**
 * @brief Linux system telemetry reader (REQ-SW-PL-041).
 *
 * Reads CPU usage (/proc/stat delta), RAM (/proc/meminfo), CPU temperature
 * (from /sys/class/hwmon temp input files) and network throughput
 * (/proc/net/dev delta) on a QTimer. Timer-based (not a worker thread) —
 * /proc + /sys reads are fast (<1 ms). Emits metricsReady() with the current
 * values on each poll.
 *
 * Linux-only: the node is guarded by HAVE_SYSTEM_MONITOR (CMake if(NOT WIN32)),
 * so this file is only compiled on non-Windows platforms.
 */
class SystemMonitorEngine : public QObject
{
    Q_OBJECT

public:
    explicit SystemMonitorEngine(QObject *parent = nullptr);
    ~SystemMonitorEngine() override;

    void setPollIntervalMs(int ms);  // 100-5000ms
    void setMetricsEnabled(bool cpu, bool ram, bool temp, bool network);
    void start();
    void stop();

signals:
    void metricsReady(const SystemMonitorMetrics &m);
    void errorOccurred(const QString &msg);

private:
    void poll();
    double readCpuPercent();
    double readRamPercent();
    double readCpuTempC();
    void readNetworkKbps(double &rxKbps, double &txKbps);

    QTimer *m_timer;
    int m_pollIntervalMs = 1000;
    bool m_cpuEnabled = true;
    bool m_ramEnabled = true;
    bool m_tempEnabled = true;
    bool m_networkEnabled = true;

    // For delta calculations
    quint64 m_prevIdle = 0;
    quint64 m_prevTotal = 0;
    quint64 m_prevNetRxBytes = 0;
    quint64 m_prevNetTxBytes = 0;
    bool m_firstRead = true;
};

#endif // SYSTEMMONITORENGINE_H
