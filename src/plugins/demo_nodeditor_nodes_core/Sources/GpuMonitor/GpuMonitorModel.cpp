#include "GpuMonitorModel.h"

#include <QJsonObject>

#include <cstring>

GpuMonitorModel::GpuMonitorModel()
{
    m_engine = new GpuMonitorEngine(this);

    connect(m_engine, &GpuMonitorEngine::metricsReady,
            this, &GpuMonitorModel::onMetricsReady);
    connect(m_engine, &GpuMonitorEngine::statusChanged,
            this, &GpuMonitorModel::onStatusChanged);
    connect(m_engine, &GpuMonitorEngine::errorOccurred,
            this, &GpuMonitorModel::onErrorOccurred);
}

GpuMonitorModel::~GpuMonitorModel()
{
    // Single shutdown path: stop() stops the timer + nvmlShutdown
    // (REQ-SW-PL-045 AC 6, REQ-SW-PL-050).
    stop();
}

void GpuMonitorModel::stop()
{
    // Idempotent: the engine's stop() is safe to call when not polling.
    if (m_engine)
        m_engine->stop();
    m_userStarted = false;
}

/// Start polling programmatically (runtime autoStart, REQ-SW-PL-048).
/// Delegates to the same logic as onStartRequested().
void GpuMonitorModel::start()
{
    m_userStarted = true;
    setPollingEnabled(true);
}

QJsonObject GpuMonitorModel::save() const
{
    QJsonObject modelJson;
    modelJson["name"] = name();
    modelJson["intervalSeconds"] = m_engine ? m_engine->intervalSeconds() : 1.0;
    return modelJson;
}

void GpuMonitorModel::load(QJsonObject const &p)
{
    if (m_engine && p.contains("intervalSeconds")) {
        m_engine->setPollIntervalMs(static_cast<int>(p["intervalSeconds"].toDouble(1.0) * 1000.0));
    }
}

unsigned int GpuMonitorModel::nPorts(QtNodes::PortType portType) const
{
    return portType == QtNodes::PortType::Out ? 1u : 0u;
}

QtNodes::NodeDataType GpuMonitorModel::dataType(QtNodes::PortType portType,
                                                QtNodes::PortIndex portIndex) const
{
    Q_UNUSED(portType);
    Q_UNUSED(portIndex);
    return SampledData().type();
}

std::shared_ptr<QtNodes::NodeData> GpuMonitorModel::outData(QtNodes::PortIndex port)
{
    Q_UNUSED(port);
    return m_lastData;
}

void GpuMonitorModel::setInData(std::shared_ptr<QtNodes::NodeData> data,
                                QtNodes::PortIndex port)
{
    Q_UNUSED(data);
    Q_UNUSED(port);
    Q_ASSERT(0);
}

void GpuMonitorModel::outputConnectionCreated(QtNodes::ConnectionId const &conId)
{
    Q_UNUSED(conId);
    ++m_connectionCount;
    setPollingEnabled(m_connectionCount > 0);
}

void GpuMonitorModel::outputConnectionDeleted(QtNodes::ConnectionId const &conId)
{
    Q_UNUSED(conId);
    if (m_connectionCount > 0)
        --m_connectionCount;
    setPollingEnabled(m_connectionCount > 0);
}

// Public slots called by GUI widget via NodeWidgetFactory

void GpuMonitorModel::onStartRequested()
{
    m_userStarted = true;
    setPollingEnabled(true);
}

void GpuMonitorModel::onStopRequested()
{
    m_userStarted = false;
    setPollingEnabled(false);
}

void GpuMonitorModel::onIntervalChanged(double seconds)
{
    if (m_engine) {
        m_engine->setPollIntervalMs(static_cast<int>(seconds * 1000.0));
    }
}

void GpuMonitorModel::onMetricsReady(const GpuMonitorEngine::Metrics &m)
{
    m_lastData = buildSampledData(m);
    // GUI widget (if present) will be updated via its own connection to engine
    emit dataUpdated(0);
}

void GpuMonitorModel::onStatusChanged(const QString &status)
{
    // GUI widget (if present) will be updated via its own connection to engine
    Q_UNUSED(status);
}

void GpuMonitorModel::onErrorOccurred(const QString &msg)
{
    // GUI widget (if present) will be updated via its own connection to engine
    Q_UNUSED(msg);
}

void GpuMonitorModel::setPollingEnabled(bool enabled)
{
    const bool shouldRun = enabled && m_userStarted;
    if (shouldRun)
        m_engine->start();
    else
        m_engine->stop();
    // GUI widget (if present) will update its own running state
}

std::shared_ptr<SampledData> GpuMonitorModel::buildSampledData(
    const GpuMonitorEngine::Metrics &m) const
{
    SampledStreamDescriptor desc;
    desc.sampleRate = m_engine ? (1.0 / m_engine->intervalSeconds()) : 1.0;
    desc.channels = {
        {QStringLiteral("gpu_util"), SampleType::FLOAT32},
        {QStringLiteral("mem_used"), SampleType::FLOAT32},
        {QStringLiteral("gpu_temp_c"), SampleType::FLOAT32},
        {QStringLiteral("power_w"), SampleType::FLOAT32},
        {QStringLiteral("fan_pct"), SampleType::FLOAT32},
        {QStringLiteral("clock_mhz"), SampleType::FLOAT32},
    };
    desc.endianness = SampleEndian::LittleEndian;
    desc.unit = QStringLiteral("normalized");
    desc.domain = QStringLiteral("gpu");
    desc.deviceId = QStringLiteral("gpu0");
    desc.sourceName = m_engine->gpuName().isEmpty()
                          ? QStringLiteral("NVIDIA GPU")
                          : m_engine->gpuName();

    // One frame of 6 FLOAT32 samples (interleaved layout).
    QByteArray buffer;
    buffer.resize(desc.bytesPerFrame());
    float *ptr = reinterpret_cast<float *>(buffer.data());
    ptr[0] = static_cast<float>(m.gpuUtil);
    ptr[1] = static_cast<float>(m.memUsedPct);
    ptr[2] = static_cast<float>(m.tempC);
    ptr[3] = static_cast<float>(m.powerW);
    ptr[4] = static_cast<float>(m.fanPct);
    ptr[5] = static_cast<float>(m.clockMhz);

    return std::make_shared<SampledData>(buffer, desc);
}