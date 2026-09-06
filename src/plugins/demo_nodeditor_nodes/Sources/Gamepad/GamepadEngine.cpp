#include "GamepadEngine.h"

#include <QTimer>

#include <fcntl.h>
#include <linux/joystick.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>

namespace {

// Normalize a raw int16 joystick axis value [-32767, 32767] to [-1.0, 1.0].
float normalizeAxis(__s16 value)
{
    return static_cast<float>(value) / 32767.0f;
}

} // namespace

GamepadEngine::GamepadEngine(QObject *parent)
    : QObject(parent)
    , m_timer(new QTimer(this))
{
    m_timer->setInterval(1000 / m_pollRateHz);
    connect(m_timer, &QTimer::timeout, this, &GamepadEngine::poll);
}

GamepadEngine::~GamepadEngine()
{
    stop();
}

void GamepadEngine::setDevicePath(const QString &path)
{
    if (m_fd >= 0) {
        emit errorOccurred(QStringLiteral("Device path cannot change while open"));
        return;
    }
    m_devicePath = path;
}

void GamepadEngine::setPollRate(int hz)
{
    m_pollRateHz = qBound(10, hz, 200);
    m_timer->setInterval(1000 / m_pollRateHz);
}

bool GamepadEngine::open()
{
    if (m_fd >= 0)
        return true;

    m_fd = ::open(m_devicePath.toUtf8().constData(), O_RDONLY | O_NONBLOCK);
    if (m_fd < 0) {
        const QString msg = QStringLiteral("Cannot open %1: %2")
                                .arg(m_devicePath, QString::fromLocal8Bit(std::strerror(errno)));
        emit errorOccurred(msg);
        return false;
    }

    int axes = 0;
    int buttons = 0;
    if (ioctl(m_fd, JSIOCGAXES, &axes) < 0 || ioctl(m_fd, JSIOCGBUTTONS, &buttons) < 0) {
        const QString msg = QStringLiteral("Cannot query joystick capabilities: %1")
                                .arg(QString::fromLocal8Bit(std::strerror(errno)));
        emit errorOccurred(msg);
        ::close(m_fd);
        m_fd = -1;
        return false;
    }

    emit statusChanged(QStringLiteral("Opened %1 (%2 axes, %3 buttons)")
                           .arg(m_devicePath)
                           .arg(axes)
                           .arg(buttons));
    return true;
}

void GamepadEngine::close()
{
    if (m_fd >= 0) {
        ::close(m_fd);
        m_fd = -1;
    }
}

void GamepadEngine::start()
{
    if (m_fd < 0 && !open())
        return;
    m_timer->start();
    emit statusChanged(QStringLiteral("Polling %1 at %2 Hz").arg(m_devicePath).arg(m_pollRateHz));
}

void GamepadEngine::stop()
{
    m_timer->stop();
    close();
}

void GamepadEngine::poll()
{
    if (m_fd < 0)
        return;

    struct js_event event;

    while (::read(m_fd, &event, sizeof(event)) > 0) {
        const unsigned char type = static_cast<unsigned char>(event.type & ~JS_EVENT_INIT);

        switch (type) {
        case JS_EVENT_AXIS:
            switch (event.number) {
            case 0: m_state.axisX = normalizeAxis(event.value); break;
            case 1: m_state.axisY = normalizeAxis(event.value); break;
            case 2: m_state.axisZ = normalizeAxis(event.value); break;
            case 3: m_state.axisRz = normalizeAxis(event.value); break;
            default: break; // extra axes beyond the 4 supported are ignored
            }
            break;
        case JS_EVENT_BUTTON:
            switch (event.number) {
            case 0: m_state.buttonA = event.value ? 1.0f : 0.0f; break;
            case 1: m_state.buttonB = event.value ? 1.0f : 0.0f; break;
            case 2: m_state.buttonX = event.value ? 1.0f : 0.0f; break;
            case 3: m_state.buttonY = event.value ? 1.0f : 0.0f; break;
            case 4: m_state.buttonLB = event.value ? 1.0f : 0.0f; break;
            case 5: m_state.buttonRB = event.value ? 1.0f : 0.0f; break;
            case 6: m_state.buttonBack = event.value ? 1.0f : 0.0f; break;
            case 7: m_state.buttonStart = event.value ? 1.0f : 0.0f; break;
            default: break; // extra buttons beyond the 8 supported are ignored
            }
            break;
        default:
            break; // JS_EVENT_INIT-only records carry no state change
        }
    }

    // Emit the current state after draining — one "frame" per poll at the
    // configured rate (matches the SampledStreamDescriptor sampleRate).
    emit stateReady(m_state);
}