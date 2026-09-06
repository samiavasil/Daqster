#ifndef GPUMONITORWIDGET_H
#define GPUMONITORWIDGET_H

#include "GpuMonitorEngine.h"

#include <QWidget>

class QDoubleSpinBox;
class QLabel;
class QPushButton;

/**
 * @brief Config UI for the GPU Monitor source node (REQ-SW-PL-045).
 *
 * Provides a polling-interval spin box (0.1–5.0 s), a Start/Stop toggle and a
 * status label showing the GPU name + current metrics. Emits signals that the
 * owning GpuMonitorModel connects to the engine.
 */
class GpuMonitorWidget : public QWidget
{
    Q_OBJECT

public:
    explicit GpuMonitorWidget(QWidget *parent = nullptr);

    /// Update the status label with the latest metrics snapshot.
    void updateMetrics(const GpuMonitorEngine::Metrics &m);

    /// Update the status label with a plain status/error string.
    void setStatusText(const QString &text);

    /// Current polling interval in seconds (0.1–5.0).
    double intervalSeconds() const;

    /// Set the polling interval in seconds (clamped to 0.1–5.0).
    void setIntervalSeconds(double seconds);

    /// Reflect the running state in the Start/Stop button label.
    void setRunning(bool running);

signals:
    void startRequested();
    void stopRequested();
    void intervalChanged(double seconds);

private:
    QDoubleSpinBox *m_intervalSpin = nullptr;
    QPushButton *m_startStopButton = nullptr;
    QLabel *m_statusLabel = nullptr;
};

#endif // GPUMONITORWIDGET_H
