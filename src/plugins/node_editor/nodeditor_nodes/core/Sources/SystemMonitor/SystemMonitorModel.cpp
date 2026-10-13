#include "SystemMonitorModel.h"

#include "LogCategories.h"

#include <QJsonObject>

using QtNodes::NodeDataType;

SystemMonitorModel::SystemMonitorModel()
{
    m_engine = new SystemMonitorEngine(this);

    // Engine → model (same thread — QTimer based).
    connect(m_engine, &SystemMonitorEngine::metricsReady,
            this, &SystemMonitorModel::onMetricsReady);
    connect(m_engine, &SystemMonitorEngine::errorOccurred,
            this, &SystemMonitorModel::onErrorOccurred);

    updateEngineConfig();
}

SystemMonitorModel::~SystemMonitorModel()
{
    // Single shutdown path: stop() stops the polling timer before the engine
    // (child) is destroyed (REQ-SW-PL-050).
    stop();
}

void SystemMonitorModel::stop()
{
    // Idempotent: the engine's stop() is safe to call when not polling.
    if (m_engine)
        m_engine->stop();
    m_userStarted = false;
}

/// Start polling programmatically (runtime autoStart, REQ-SW-PL-048).
/// Delegates to the same logic as onStartRequested().
void SystemMonitorModel::start()
{
    m_userStarted = true;
    setPollingEnabled(m_connectionCount > 0);
}

QJsonObject SystemMonitorModel::save() const
{
    QJsonObject modelJson = QtNodes::NodeDelegateModel::save();

    // Config values are saved by the GUI widget via NodeWidgetFactory
    // Core model stores only the polling interval for the engine
    modelJson["pollIntervalSec"] = m_engine ? m_engine->pollIntervalMs() / 1000.0 : 1.0;

    return modelJson;
}

void SystemMonitorModel::load(QJsonObject const &p)
{
    if (m_engine) {
        m_engine->setPollIntervalMs(static_cast<int>(p.value("pollIntervalSec").toDouble(1.0) * 1000.0));
    }
    // Other config (cpu/ram/temp/network enabled) is applied by GUI widget
    updateEngineConfig();
}

unsigned int SystemMonitorModel::nPorts(QtNodes::PortType portType) const
{
    return portType == QtNodes::PortType::Out ? 1 : 0;
}

QtNodes::NodeDataType SystemMonitorModel::dataType(QtNodes::PortType portType,
                                                   QtNodes::PortIndex portIndex) const
{
    Q_UNUSED(portType);
    Q_UNUSED(portIndex);
    return SampledData().type();
}

std::shared_ptr<QtNodes::NodeData> SystemMonitorModel::outData(QtNodes::PortIndex port)
{
    Q_UNUSED(port);
    return m_output;
}

void SystemMonitorModel::setInData(std::shared_ptr<QtNodes::NodeData> data,
                                   QtNodes::PortIndex port)
{
    Q_UNUSED(data);
    Q_UNUSED(port);
    Q_ASSERT(0);
}

// ── Connection-count gating (model of PlutoSdrModel) ────────────────────────

void SystemMonitorModel::outputConnectionCreated(QtNodes::ConnectionId const &conId)
{
    Q_UNUSED(conId);
    ++m_connectionCount;
    setPollingEnabled(m_userStarted && m_connectionCount > 0);
}

void SystemMonitorModel::outputConnectionDeleted(QtNodes::ConnectionId const &conId)
{
    Q_UNUSED(conId);
    if (m_connectionCount > 0)
        --m_connectionCount;
    setPollingEnabled(m_userStarted && m_connectionCount > 0);
}

// Public slots called by GUI widget via NodeWidgetFactory

void SystemMonitorModel::onStartRequested()
{
    m_userStarted = true;
    setPollingEnabled(m_connectionCount > 0);
}

void SystemMonitorModel::onStopRequested()
{
    m_userStarted = false;
    m_engine->stop();
    // GUI widget will update its own status
}

void SystemMonitorModel::onIntervalChanged(double sec)
{
    if (m_engine) {
        m_engine->setPollIntervalMs(static_cast<int>(sec * 1000.0));
    }
}

void SystemMonitorModel::onMetricsChanged(bool cpu, bool ram, bool temp, bool network)
{
    if (m_engine) {
        m_engine->setMetricsEnabled(cpu, ram, temp, network);
    }
}

// ── Engine slots ────────────────────────────────────────────────────────────

void SystemMonitorModel::onMetricsReady(const SystemMonitorMetrics &m)
{
    // System telemetry IS sampled data: 5 FLOAT32 channels, one "frame" per
    // poll — exactly what SampledStreamDescriptor describes (REQ-SW-PL-041 §1).
    SampledStreamDescriptor desc;
    desc.sampleRate = m_engine ? (1000.0 / m_engine->pollIntervalMs()) : 1.0;
    desc.channels = {
        {QStringLiteral("cpu_percent"), SampleType::FLOAT32},
        {QStringLiteral("ram_percent"), SampleType::FLOAT32},
        {QStringLiteral("cpu_temp_c"), SampleType::FLOAT32},
        {QStringLiteral("net_rx_kbps"), SampleType::FLOAT32},
        {QStringLiteral("net_tx_kbps"), SampleType::FLOAT32},
    };
    desc.endianness = SampleEndian::LittleEndian;
    desc.unit = QStringLiteral("percent");
    desc.domain = QStringLiteral("system");
    desc.deviceId = QStringLiteral("sysmon");
    desc.sourceName = QStringLiteral("Linux System Monitor");

    // One interleaved frame of 5 FLOAT32 values.
    QByteArray buffer;
    buffer.resize(5 * static_cast<int>(sizeof(float)));
    float *ptr = reinterpret_cast<float *>(buffer.data());
    ptr[0] = static_cast<float>(m.cpuPercent);
    ptr[1] = static_cast<float>(m.ramPercent);
    ptr[2] = static_cast<float>(m.cpuTempC);
    ptr[3] = static_cast<float>(m.netRxKbps);
    ptr[4] = static_cast<float>(m.netTxKbps);

    m_output = std::make_shared<SampledData>(buffer, desc);

    // GUI widget (if present) will be updated via its own connection to engine
    emit dataUpdated(0);
}

void SystemMonitorModel::onErrorOccurred(const QString &message)
{
    // GUI widget (if present) will be updated via its own connection to engine
    Q_UNUSED(message);
}

// ── Helpers ─────────────────────────────────────────────────────────────────

void SystemMonitorModel::updateEngineConfig()
{
    // Configuration is applied by GUI widget via NodeWidgetFactory
    // Core model just ensures engine is running if needed
    if (m_userStarted && m_connectionCount > 0) {
        m_engine->start();
    }
}

void SystemMonitorModel::setPollingEnabled(bool enabled)
{
    if (enabled)
        m_engine->start();
    else
        m_engine->stop();
}
