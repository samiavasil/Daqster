#ifndef GAMEPADWIDGET_H
#define GAMEPADWIDGET_H

#include <QWidget>

class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;

/**
 * @brief Config UI for the Gamepad source node (REQ-SW-PL-042).
 *
 * Device path (default /dev/input/js0), poll rate (30–120 Hz, default 60),
 * 4 axis value labels (X/Y/Z/Rz), 8 button state labels (green = pressed,
 * gray = released), a Start/Stop toggle and a status label. Emits
 * startRequested/stopRequested to the model and devicePathChanged/
 * pollRateChanged whenever a config control changes.
 */
class GamepadWidget : public QWidget
{
    Q_OBJECT

public:
    explicit GamepadWidget(QWidget *parent = nullptr);

    // ── Config accessors (used by the model for save/load) ────────────────
    QString devicePath() const;
    int pollRateHz() const;

    void setDevicePath(const QString &path);
    void setPollRateHz(int hz);

    bool isStarted() const { return m_started; }

signals:
    void startRequested();
    void stopRequested();
    void devicePathChanged(const QString &path);
    void pollRateChanged(int hz);

public slots:
    void setStatus(const QString &status);
    void setAxisValues(float x, float y, float z, float rz);
    void setButtonStates(float a, float b, float x, float y,
                         float lb, float rb, float back, float start);

private slots:
    void onStartStopClicked();

private:
    QLabel *makeAxisLabel();
    QLabel *makeButtonLabel(const QString &name);

    QLineEdit *m_deviceEdit = nullptr;
    QSpinBox *m_pollRateSpin = nullptr;
    QLabel *m_axisXLabel = nullptr;
    QLabel *m_axisYLabel = nullptr;
    QLabel *m_axisZLabel = nullptr;
    QLabel *m_axisRzLabel = nullptr;
    QLabel *m_buttonALabel = nullptr;
    QLabel *m_buttonBLabel = nullptr;
    QLabel *m_buttonXLabel = nullptr;
    QLabel *m_buttonYLabel = nullptr;
    QLabel *m_buttonLBLabel = nullptr;
    QLabel *m_buttonRBLabel = nullptr;
    QLabel *m_buttonBackLabel = nullptr;
    QLabel *m_buttonStartLabel = nullptr;
    QPushButton *m_startStopButton = nullptr;
    QLabel *m_statusLabel = nullptr;
    bool m_started = false;
};

#endif // GAMEPADWIDGET_H