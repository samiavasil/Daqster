#ifndef GAMEPADENGINE_H
#define GAMEPADENGINE_H

#include <QObject>
#include <QString>

#include <QtGlobal>

class QTimer;

/**
 * @brief Current gamepad state (REQ-SW-PL-042).
 *
 * Top-level struct (not nested) so Qt moc can resolve it in the
 * stateReady() signal / onStateReady() slot signature.
 *
 * Axes are normalized from int16 [-32767, 32767] to float [-1.0, 1.0].
 * Buttons are 0.0 (pressed) or 1.0 (released) — Linux joystick convention.
 */
struct GamepadState {
    float axisX = 0.0f;
    float axisY = 0.0f;
    float axisZ = 0.0f;
    float axisRz = 0.0f;
    float buttonA = 0.0f;
    float buttonB = 0.0f;
    float buttonX = 0.0f;
    float buttonY = 0.0f;
    float buttonLB = 0.0f;
    float buttonRB = 0.0f;
    float buttonBack = 0.0f;
    float buttonStart = 0.0f;
};

/**
 * @brief Linux joystick API wrapper (REQ-SW-PL-042).
 *
 * Opens the joystick device (default /dev/input/js0) with O_RDONLY|O_NONBLOCK,
 * drains pending struct js_event records on a QTimer poll (default 60 Hz),
 * updates the current axis/button state and emits stateReady() after each
 * drain. Timer-based (not a worker thread) — a full drain is a few syscalls
 * on a small buffer, well under 1 ms at 60 Hz.
 *
 * Linux-only: the node is guarded by HAVE_GAMEPAD (CMake if(NOT WIN32)), so
 * this file is only compiled on non-Windows platforms.
 */
class GamepadEngine : public QObject
{
    Q_OBJECT

public:
    explicit GamepadEngine(QObject *parent = nullptr);
    ~GamepadEngine() override;

    void setDevicePath(const QString &path);  // default "/dev/input/js0"
    void setPollRate(int hz);                 // default 60
    bool open();                              // open + non-blocking
    void close();
    void start();                             // QTimer start
    void stop();                              // QTimer stop + close

    const GamepadState &state() const { return m_state; }

signals:
    void stateReady(const GamepadState &s);
    void statusChanged(const QString &status);
    void errorOccurred(const QString &msg);

private:
    void poll();  // read pending js_events, update state, emit stateReady

    QString m_devicePath = QStringLiteral("/dev/input/js0");
    int m_fd = -1;
    QTimer *m_timer = nullptr;
    int m_pollRateHz = 60;
    GamepadState m_state;
};

#endif // GAMEPADENGINE_H