#ifndef SYSTEMMONITORWIDGET_H
#define SYSTEMMONITORWIDGET_H

#include <QWidget>

class QCheckBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;

/**
 * @brief Config UI for the System Monitor source node (REQ-SW-PL-041).
 *
 * Polling interval (0.1–5.0 s, default 1.0), metric enable checkboxes
 * (CPU/RAM/Temp/Network), a Start/Stop toggle and a status label showing the
 * current values or "Idle". Emits startRequested/stopRequested to the model
 * and intervalChanged/metricsChanged whenever a config control changes.
 */
class SystemMonitorWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SystemMonitorWidget(QWidget *parent = nullptr);

    // ── Config accessors (used by the model for save/load) ────────────────
    double pollIntervalSec() const;
    bool cpuEnabled() const;
    bool ramEnabled() const;
    bool tempEnabled() const;
    bool networkEnabled() const;

    void setPollIntervalSec(double sec);
    void setCpuEnabled(bool enabled);
    void setRamEnabled(bool enabled);
    void setTempEnabled(bool enabled);
    void setNetworkEnabled(bool enabled);

    bool isStarted() const { return m_started; }

signals:
    void startRequested();
    void stopRequested();
    void intervalChanged(double sec);
    void metricsChanged(bool cpu, bool ram, bool temp, bool network);

public slots:
    void setStatus(const QString &status);

private slots:
    void onStartStopClicked();

private:
    QDoubleSpinBox *m_intervalSpin = nullptr;
    QCheckBox *m_cpuCheck = nullptr;
    QCheckBox *m_ramCheck = nullptr;
    QCheckBox *m_tempCheck = nullptr;
    QCheckBox *m_networkCheck = nullptr;
    QPushButton *m_startStopButton = nullptr;
    QLabel *m_statusLabel = nullptr;
    bool m_started = false;
};

#endif // SYSTEMMONITORWIDGET_H
