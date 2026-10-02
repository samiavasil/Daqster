#include "SystemMonitorEngine.h"

#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QTimer>

#include <algorithm>

namespace {

// Read the whole file as a trimmed string; empty on failure.
QString readFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return QString();
    QTextStream in(&file);
    return in.readAll().trimmed();
}

// Parse a "key: value" line from /proc/meminfo; returns -1 if not found.
double parseMeminfoValue(const QString &content, const QString &key)
{
    const QStringList lines = content.split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        if (line.startsWith(key)) {
            const QStringList parts = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
            if (parts.size() >= 2)
                return parts.at(1).toDouble();
        }
    }
    return -1.0;
}

} // namespace

SystemMonitorEngine::SystemMonitorEngine(QObject *parent)
    : QObject(parent)
    , m_timer(new QTimer(this))
{
    m_timer->setInterval(m_pollIntervalMs);
    connect(m_timer, &QTimer::timeout, this, &SystemMonitorEngine::poll);
}

SystemMonitorEngine::~SystemMonitorEngine()
{
    stop();
}

void SystemMonitorEngine::setPollIntervalMs(int ms)
{
    m_pollIntervalMs = std::clamp(ms, 100, 5000);
    m_timer->setInterval(m_pollIntervalMs);
}

void SystemMonitorEngine::setMetricsEnabled(bool cpu, bool ram, bool temp, bool network)
{
    m_cpuEnabled = cpu;
    m_ramEnabled = ram;
    m_tempEnabled = temp;
    m_networkEnabled = network;
}

void SystemMonitorEngine::start()
{
    m_firstRead = true;
    m_prevIdle = 0;
    m_prevTotal = 0;
    m_prevNetRxBytes = 0;
    m_prevNetTxBytes = 0;
    poll();
    m_timer->start();
}

void SystemMonitorEngine::stop()
{
    m_timer->stop();
}

void SystemMonitorEngine::poll()
{
    SystemMonitorMetrics m;
    if (m_cpuEnabled)
        m.cpuPercent = readCpuPercent();
    if (m_ramEnabled)
        m.ramPercent = readRamPercent();
    if (m_tempEnabled)
        m.cpuTempC = readCpuTempC();
    if (m_networkEnabled)
        readNetworkKbps(m.netRxKbps, m.netTxKbps);
    emit metricsReady(m);
}

double SystemMonitorEngine::readCpuPercent()
{
    const QString content = readFile(QStringLiteral("/proc/stat"));
    if (content.isEmpty()) {
        emit errorOccurred(QStringLiteral("Cannot read /proc/stat"));
        return 0.0;
    }

    const QString firstLine = content.section(QLatin1Char('\n'), 0, 0);
    const QStringList fields = firstLine.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    // fields: cpu user nice system idle iowait irq softirq steal
    if (fields.size() < 8) {
        emit errorOccurred(QStringLiteral("Unexpected /proc/stat format"));
        return 0.0;
    }

    const quint64 user = fields.at(1).toULongLong();
    const quint64 nice = fields.at(2).toULongLong();
    const quint64 system = fields.at(3).toULongLong();
    const quint64 idle = fields.at(4).toULongLong();
    const quint64 iowait = fields.at(5).toULongLong();
    const quint64 irq = fields.at(6).toULongLong();
    const quint64 softirq = fields.at(7).toULongLong();
    const quint64 steal = fields.size() > 8 ? fields.at(8).toULongLong() : 0;

    const quint64 total = user + nice + system + idle + iowait + irq + softirq + steal;

    if (m_firstRead) {
        m_prevIdle = idle;
        m_prevTotal = total;
        return 0.0;
    }

    const quint64 idleDelta = idle > m_prevIdle ? idle - m_prevIdle : 0;
    const quint64 totalDelta = total > m_prevTotal ? total - m_prevTotal : 0;

    m_prevIdle = idle;
    m_prevTotal = total;

    if (totalDelta == 0)
        return 0.0;

    return (1.0 - static_cast<double>(idleDelta) / static_cast<double>(totalDelta)) * 100.0;
}

double SystemMonitorEngine::readRamPercent()
{
    const QString content = readFile(QStringLiteral("/proc/meminfo"));
    if (content.isEmpty()) {
        emit errorOccurred(QStringLiteral("Cannot read /proc/meminfo"));
        return 0.0;
    }

    const double memTotal = parseMeminfoValue(content, QStringLiteral("MemTotal:"));
    const double memAvailable = parseMeminfoValue(content, QStringLiteral("MemAvailable:"));
    if (memTotal <= 0.0 || memAvailable < 0.0) {
        emit errorOccurred(QStringLiteral("Unexpected /proc/meminfo format"));
        return 0.0;
    }

    return (1.0 - memAvailable / memTotal) * 100.0;
}

double SystemMonitorEngine::readCpuTempC()
{
    // Enumerate /sys/class/hwmon/hwmon*/temp*_input, read the first found.
    const QDir hwmonDir(QStringLiteral("/sys/class/hwmon"));
    const QStringList hwmonEntries = hwmonDir.entryList(
        {QStringLiteral("hwmon*")}, QDir::Dirs | QDir::NoDotAndDotDot);

    for (const QString &hwmon : hwmonEntries) {
        const QDir dir(hwmonDir.filePath(hwmon));
        const QStringList tempInputs = dir.entryList(
            {QStringLiteral("temp*_input")}, QDir::Files | QDir::NoDotAndDotDot);
        if (tempInputs.isEmpty())
            continue;

        const QString value = readFile(dir.filePath(tempInputs.first()));
        if (value.isEmpty())
            continue;

        // Value is in millidegrees Celsius.
        return value.toDouble() / 1000.0;
    }

    return 0.0; // no hwmon temp sensor found
}

void SystemMonitorEngine::readNetworkKbps(double &rxKbps, double &txKbps)
{
    rxKbps = 0.0;
    txKbps = 0.0;

    const QString content = readFile(QStringLiteral("/proc/net/dev"));
    if (content.isEmpty()) {
        emit errorOccurred(QStringLiteral("Cannot read /proc/net/dev"));
        return;
    }

    quint64 rxBytes = 0;
    quint64 txBytes = 0;
    bool found = false;

    const QStringList lines = content.split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        const QString trimmed = line.trimmed();
        if (trimmed.isEmpty() || trimmed.startsWith(QLatin1String("Inter-|")))
            continue;
        if (trimmed.startsWith(QLatin1String("face |")))
            continue;

        // Format: "iface: rx_bytes rx_packets ... tx_bytes tx_packets ..."
        const int colon = trimmed.indexOf(QLatin1Char(':'));
        if (colon < 0)
            continue;

        const QString iface = trimmed.left(colon).trimmed();
        if (iface == QLatin1String("lo"))
            continue; // skip loopback

        const QStringList fields = trimmed.mid(colon + 1)
                                       .split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (fields.size() < 9)
            continue;

        rxBytes += fields.at(0).toULongLong();
        txBytes += fields.at(8).toULongLong();
        found = true;
    }

    if (!found)
        return;

    if (m_firstRead) {
        m_prevNetRxBytes = rxBytes;
        m_prevNetTxBytes = txBytes;
        return;
    }

    const double intervalSec = static_cast<double>(m_pollIntervalMs) / 1000.0;
    if (intervalSec <= 0.0)
        return;

    const quint64 rxDelta = rxBytes > m_prevNetRxBytes ? rxBytes - m_prevNetRxBytes : 0;
    const quint64 txDelta = txBytes > m_prevNetTxBytes ? txBytes - m_prevNetTxBytes : 0;

    m_prevNetRxBytes = rxBytes;
    m_prevNetTxBytes = txBytes;

    // bytes/s → kbps (kilobits per second): bytes * 8 / 1000 / interval.
    rxKbps = (static_cast<double>(rxDelta) * 8.0 / 1000.0) / intervalSec;
    txKbps = (static_cast<double>(txDelta) * 8.0 / 1000.0) / intervalSec;
}
