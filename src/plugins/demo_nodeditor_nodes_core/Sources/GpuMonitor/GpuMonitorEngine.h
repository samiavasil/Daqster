#ifndef GPUMONITORENGINE_H
#define GPUMONITORENGINE_H

#include <QObject>
#include <QString>

class QTimer;

/**
 * @brief NVML wrapper that polls NVIDIA GPU telemetry on a QTimer.
 *
 * REQ-SW-PL-045: reads GPU utilization / memory / temperature / power / fan /
 * clock via the NVIDIA Management Library (NVML) and emits a Metrics struct on
 * each poll. All NVML calls are guarded by #ifdef HAVE_NVML — without NVML the
 * class compiles as a no-op (init() returns false, poll() emits nothing).
 *
 * The engine owns a QTimer; start() begins polling, stop() stops the timer and
 * calls nvmlShutdown(). The caller (GpuMonitorModel) owns the engine and is
 * responsible for calling stop()/shutdown() on destruction.
 */
class GpuMonitorEngine : public QObject
{
    Q_OBJECT

public:
    /// One snapshot of GPU telemetry (all physical units).
    struct Metrics {
        double gpuUtil = 0.0;    // GPU utilization %
        double memUsedPct = 0.0; // memory used % (used / total * 100)
        double tempC = 0.0;      // GPU die temperature °C
        double powerW = 0.0;     // power draw W
        double fanPct = 0.0;     // fan speed %
        double clockMhz = 0.0;   // graphics clock MHz
    };

    explicit GpuMonitorEngine(QObject *parent = nullptr);
    ~GpuMonitorEngine() override;

    /// Set the polling interval in ms (clamped to 100..5000).
    void setPollIntervalMs(int ms);

    /// Initialize NVML and acquire the handle for GPU 0. Returns false on
    /// failure (or when NVML is not compiled in).
    bool init();

    /// Release NVML resources (nvmlShutdown). Safe to call multiple times.
    void shutdown();

    /// Start polling: init() if needed, then start the timer.
    void start();

    /// Stop polling and release NVML resources.
    void stop();

    /// Human-readable GPU name (e.g. "NVIDIA GeForce RTX 3070 Laptop GPU").
    QString gpuName() const { return m_gpuName; }

signals:
    void metricsReady(const GpuMonitorEngine::Metrics &m);
    void statusChanged(const QString &status);
    void errorOccurred(const QString &msg);

private:
    void poll();

    QTimer *m_timer;
    int m_pollIntervalMs = 1000;
    void *m_device = nullptr; // nvmlDevice_t (opaque handle)
    bool m_initialized = false;
    QString m_gpuName;
};

#endif // GPUMONITORENGINE_H
