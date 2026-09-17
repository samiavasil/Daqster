#include "SystemMonitorModel.h"

#include "LogCategories.h"

#include <QJsonObject>

using QtNodes::NodeDataType;

SystemMonitorModel::SystemMonitorModel()
{
    m_engine = new SystemMonitorEngine(this);
    m_widget = new SystemMonitorWidget();

    // Widget → model (GUI thread).
    connect(m_widget, &SystemMonitorWidget::startRequested,
            this, &SystemMonitorModel::onStartRequested);
    connect(m_widget, &SystemMonitorWidget::stopRequested,
            this, &SystemMonitorModel::onStopRequested);
    connect(m_widget, &SystemMonitorWidget::intervalChanged,
            this, &SystemMonitorModel::onIntervalChanged);
    connect(m_widget, &SystemMonitorWidget::metricsChanged,
            this, &SystemMonitorModel::onMetricsChanged);

    // Engine → model (same thread — QTimer based).
    connect(m_engine, &SystemMonitorEngine::metricsReady,
            this, &SystemMonitorModel::onMetricsReady);
    connect(m_engine, &SystemMonitorEngine::errorOccurred,
            this, &SystemMonitorModel::onErrorOccurred);

    updateEngineConfig();
}

SystemMonitorModel::~SystemMonitorModel()
{
    // Stop the polling timer cleanly before the engine (child) is destroyed.
    if (m_engine)
        m_engine->stop();

    // Widget lifetime is owned by the node/view framework.
    m_widget = nullptr;
}

QJsonObject SystemMonitorModel::save() const
{
    QJsonObject modelJson = QtNodes::NodeDelegateModel::save();

    modelJson["pollIntervalSec"] = m_widget->pollIntervalSec();
    modelJson["cpuEnabled"] = m_widget->cpuEnabled();
    modelJson["ramEnabled"] = m_widget->ramEnabled();
    modelJson["tempEnabled"] = m_widget->tempEnabled();
    modelJson["networkEnabled"] = m_widget->networkEnabled();

    return modelJson;
}

void SystemMonitorModel::load(QJsonObject const &p)
{
    m_widget->setPollIntervalSec(p.value("pollIntervalSec").toDouble(1.0));
    m_widget->setCpuEnabled(p.value("cpuEnabled").toBool(true));
    m_widget->setRamEnabled(p.value("ramEnabled").toBool(true));
    m_widget->setTempEnabled(p.value("tempEnabled").toBool(true));
    m_widget->setNetworkEnabled(p.value("networkEnabled").toBool(true));

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

QWidget *SystemMonitorModel::embeddedWidget()
{
    return m_widget;
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

// ── Widget slots ────────────────────────────────────────────────────────────

void SystemMonitorModel::onStartRequested()
{
    m_userStarted = true;
    setPollingEnabled(m_connectionCount > 0);
}

void SystemMonitorModel::onStopRequested()
{
    m_userStarted = false;
    m_engine->stop();
    m_widget->setStatus(QStringLiteral("Idle"));
}

void SystemMonitorModel::onIntervalChanged(double sec)
{
    Q_UNUSED(sec);
    updateEngineConfig();
}

void SystemMonitorModel::onMetricsChanged(bool cpu, bool ram, bool temp, bool network)
{
    Q_UNUSED(cpu);
    Q_UNUSED(ram);
    Q_UNUSED(temp);
    Q_UNUSED(network);
    updateEngineConfig();
}

// ── Engine slots ────────────────────────────────────────────────────────────

void SystemMonitorModel::onMetricsReady(const SystemMonitorMetrics &m)
{
    // System telemetry IS sampled data: 5 FLOAT32 channels, one "frame" per
    // poll — exactly what SampledStreamDescriptor describes (REQ-SW-PL-041 §1).
    SampledStreamDescriptor desc;
    desc.sampleRate = 1.0 / m_widget->pollIntervalSec();
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

    m_widget->setStatus(QStringLiteral("CPU %1%  RAM %2%  Temp %3°C  RX %4 kbps  TX %5 kbps")
                            .arg(m.cpuPercent, 0, 'f', 1)
                            .arg(m.ramPercent, 0, 'f', 1)
                            .arg(m.cpuTempC, 0, 'f', 1)
                            .arg(m.netRxKbps, 0, 'f', 1)
                            .arg(m.netTxKbps, 0, 'f', 1));

    emit dataUpdated(0);
}

void SystemMonitorModel::onErrorOccurred(const QString &message)
{
    m_widget->setStatus(message);
}

// ── Helpers ─────────────────────────────────────────────────────────────────

void SystemMonitorModel::updateEngineConfig()
{
    m_engine->setPollIntervalMs(static_cast<int>(m_widget->pollIntervalSec() * 1000.0));
    m_engine->setMetricsEnabled(m_widget->cpuEnabled(), m_widget->ramEnabled(),
                                m_widget->tempEnabled(), m_widget->networkEnabled());
}

void SystemMonitorModel::setPollingEnabled(bool enabled)
{
    if (enabled)
        m_engine->start();
    else
        m_engine->stop();
}
