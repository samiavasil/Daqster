#ifndef JACKDETECTWIDGET_H
#define JACKDETECTWIDGET_H

#include "JackDetectEngine.h"

#include <QWidget>

class QDoubleSpinBox;
class QLabel;
class QPushButton;

/**
 * @brief Config UI for the Jack Detect source node (REQ-SW-PL-046).
 *
 * Provides a polling-interval spin box (0.1–5.0 s, default 0.5), a Start/Stop
 * toggle and a status label listing the detected jacks and their states
 * (e.g. "Headphone: No | Speaker: Yes | Internal Mic: No"). Emits signals that
 * the owning JackDetectModel connects to the engine.
 */
class JackDetectWidget : public QWidget
{
    Q_OBJECT

public:
    explicit JackDetectWidget(QWidget *parent = nullptr);

    /// Update the status label with the latest jack states.
    void setJacks(const QVector<JackDetectEngine::JackState> &jacks);

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

#endif // JACKDETECTWIDGET_H