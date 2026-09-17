#include "GpuMonitorModel.h"

#include <QJsonObject>

#include <cstring>

GpuMonitorModel::GpuMonitorModel()
{
    m_engine = new GpuMonitorEngine(this);
    m_widget = new GpuMonitorWidget;

    connect(m_widget, &GpuMonitorWidget::startRequested,
            this, &GpuMonitorModel::onStartRequested);
    connect(m_widget, &GpuMonitorWidget::stopRequested,
            this, &GpuMonitorModel::onStopRequested);
    connect(m_widget, &GpuMonitorWidget::intervalChanged,
            this, &GpuMonitorModel::onIntervalChanged);

    connect(m_engine, &GpuMonitorEngine::metricsReady,
            this, &GpuMonitorModel::onMetricsReady);
    connect(m_engine, &GpuMonitorEngine::statusChanged,
            this, &GpuMonitorModel::onStatusChanged);
    connect(m_engine, &GpuMonitorEngine::errorOccurred,
            this, &GpuMonitorModel::onErrorOccurred);
}

GpuMonitorModel::~GpuMonitorModel()
{
    // Clean shutdown: stop the timer + nvmlShutdown (REQ-SW-PL-045 AC 6).
    m_engine->stop();
    m_widget = nullptr; // owned by the node/view framework
}

QJsonObject GpuMonitorModel::save() const
{
    QJsonObject modelJson;
    modelJson["name"] = name();
    modelJson["intervalSeconds"] = m_widget->intervalSeconds();
    return modelJson;
}

void GpuMonitorModel::load(QJsonObject const &p)
{
    if (p.contains("intervalSeconds"))
        m_widget->setIntervalSeconds(p["intervalSeconds"].toDouble(1.0));
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

QWidget *GpuMonitorModel::embeddedWidget()
{
    return m_widget;
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
    m_engine->setPollIntervalMs(static_cast<int>(seconds * 1000.0));
}

void GpuMonitorModel::onMetricsReady(const GpuMonitorEngine::Metrics &m)
{
    m_lastData = buildSampledData(m);
    m_widget->updateMetrics(m);
    emit dataUpdated(0);
}

void GpuMonitorModel::onStatusChanged(const QString &status)
{
    m_widget->setStatusText(status);
}

void GpuMonitorModel::onErrorOccurred(const QString &msg)
{
    m_widget->setStatusText(msg);
    m_widget->setRunning(false);
}

void GpuMonitorModel::setPollingEnabled(bool enabled)
{
    const bool shouldRun = enabled && m_userStarted;
    if (shouldRun)
        m_engine->start();
    else
        m_engine->stop();
    m_widget->setRunning(shouldRun);
}

std::shared_ptr<SampledData> GpuMonitorModel::buildSampledData(
    const GpuMonitorEngine::Metrics &m) const
{
    SampledStreamDescriptor desc;
    desc.sampleRate = 1.0 / m_widget->intervalSeconds();
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
