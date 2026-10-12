#include "GpuMonitorEngine.h"

#include <QTimer>

#ifdef HAVE_NVML
#include <nvml.h>
#endif

GpuMonitorEngine::GpuMonitorEngine(QObject *parent)
    : QObject(parent)
    , m_timer(new QTimer(this))
{
    m_timer->setTimerType(Qt::PreciseTimer);
    connect(m_timer, &QTimer::timeout, this, &GpuMonitorEngine::poll);
}

GpuMonitorEngine::~GpuMonitorEngine()
{
    stop();
}

void GpuMonitorEngine::setPollIntervalMs(int ms)
{
    m_pollIntervalMs = qBound(100, ms, 5000);
    if (m_timer->isActive())
        m_timer->start(m_pollIntervalMs);
}

bool GpuMonitorEngine::init()
{
#ifdef HAVE_NVML
    if (m_initialized)
        return true;

    nvmlReturn_t result = nvmlInit();
    if (result != NVML_SUCCESS) {
        emit errorOccurred(QStringLiteral("NVML init failed: %1")
                               .arg(QString::fromUtf8(nvmlErrorString(result))));
        return false;
    }

    nvmlDevice_t device = nullptr;
    result = nvmlDeviceGetHandleByIndex(0, &device);
    if (result != NVML_SUCCESS) {
        emit errorOccurred(QStringLiteral("NVML device handle failed: %1")
                               .arg(QString::fromUtf8(nvmlErrorString(result))));
        nvmlShutdown();
        return false;
    }
    m_device = device;

    char name[NVML_DEVICE_NAME_BUFFER_SIZE] = {0};
    if (nvmlDeviceGetName(device, name, sizeof(name)) == NVML_SUCCESS)
        m_gpuName = QString::fromUtf8(name);

    m_initialized = true;
    emit statusChanged(QStringLiteral("GPU: %1").arg(m_gpuName));
    return true;
#else
    Q_UNUSED(m_device);
    emit errorOccurred(QStringLiteral("NVML not available (built without HAVE_NVML)"));
    return false;
#endif
}

void GpuMonitorEngine::shutdown()
{
#ifdef HAVE_NVML
    if (m_initialized) {
        nvmlShutdown();
        m_initialized = false;
        m_device = nullptr;
    }
#else
    m_initialized = false;
    m_device = nullptr;
#endif
}

void GpuMonitorEngine::start()
{
    if (!m_initialized && !init())
        return;
    m_timer->start(m_pollIntervalMs);
    emit statusChanged(QStringLiteral("Polling every %1 ms").arg(m_pollIntervalMs));
}

void GpuMonitorEngine::stop()
{
    m_timer->stop();
    shutdown();
}

void GpuMonitorEngine::poll()
{
#ifdef HAVE_NVML
    if (!m_initialized || !m_device)
        return;

    Metrics m;

    nvmlUtilization_t util = {};
    if (nvmlDeviceGetUtilizationRates(static_cast<nvmlDevice_t>(m_device), &util) == NVML_SUCCESS)
        m.gpuUtil = static_cast<double>(util.gpu);

    nvmlMemory_t mem = {};
    if (nvmlDeviceGetMemoryInfo(static_cast<nvmlDevice_t>(m_device), &mem) == NVML_SUCCESS
        && mem.total > 0)
        m.memUsedPct = static_cast<double>(mem.used) / static_cast<double>(mem.total) * 100.0;

    unsigned int temp = 0;
    if (nvmlDeviceGetTemperature(static_cast<nvmlDevice_t>(m_device),
                                 NVML_TEMPERATURE_GPU, &temp) == NVML_SUCCESS)
        m.tempC = static_cast<double>(temp);

    unsigned int power = 0;
    if (nvmlDeviceGetPowerUsage(static_cast<nvmlDevice_t>(m_device), &power) == NVML_SUCCESS)
        m.powerW = static_cast<double>(power) / 1000.0; // mW -> W

    unsigned int fan = 0;
    if (nvmlDeviceGetFanSpeed(static_cast<nvmlDevice_t>(m_device), &fan) == NVML_SUCCESS)
        m.fanPct = static_cast<double>(fan);

    unsigned int clock = 0;
    if (nvmlDeviceGetClockInfo(static_cast<nvmlDevice_t>(m_device),
                               NVML_CLOCK_GRAPHICS, &clock) == NVML_SUCCESS)
        m.clockMhz = static_cast<double>(clock);

    emit metricsReady(m);
#else
    Q_UNUSED(m_device);
#endif
}
