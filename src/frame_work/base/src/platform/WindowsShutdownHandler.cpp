#include "WindowsShutdownHandler.h"

#include <QDebug>

#ifdef Q_OS_WIN
WindowsShutdownHandler* WindowsShutdownHandler::s_instance = nullptr;
#endif

WindowsShutdownHandler::WindowsShutdownHandler(QObject *parent)
    : ShutdownHandler(parent)
#ifdef Q_OS_WIN
    , m_notifier(nullptr)
#endif
{
#ifdef Q_OS_WIN
    s_instance = this;
#endif
}

WindowsShutdownHandler::~WindowsShutdownHandler()
{
#ifdef Q_OS_WIN
    s_instance = nullptr;
#endif
}

bool WindowsShutdownHandler::initialize()
{
#ifdef Q_OS_WIN
    // Setup Windows console control handler
    if (!SetConsoleCtrlHandler(consoleCtrlHandler, TRUE)) {
        qWarning() << "Failed to set console control handler";
        return false;
    }
    qDebug() << "Windows shutdown handler initialized (Console Ctrl events)";
#else
    qDebug() << "WindowsShutdownHandler is a no-op on non-Windows platforms";
#endif

    return true;
}

#ifdef Q_OS_WIN
BOOL WINAPI WindowsShutdownHandler::consoleCtrlHandler(DWORD signal)
{
    if (s_instance) {
        QString signalName;
        switch (signal) {
            case CTRL_C_EVENT:
                signalName = "Ctrl+C";
                break;
            case CTRL_BREAK_EVENT:
                signalName = "Ctrl+Break";
                break;
            case CTRL_CLOSE_EVENT:
                signalName = "Console Close";
                break;
            case CTRL_LOGOFF_EVENT:
                signalName = "Logoff";
                break;
            case CTRL_SHUTDOWN_EVENT:
                signalName = "Shutdown";
                break;
            default:
                signalName = QString("Unknown(%1)").arg(signal);
        }

        qDebug() << "\nReceived Windows console event:" << signalName;
        QMetaObject::invokeMethod(s_instance, "shutdownRequested", Qt::QueuedConnection);
        return TRUE;
    }
    return FALSE;
}
#endif
