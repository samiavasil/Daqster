#pragma once

#include "ShutdownHandler.h"

#ifdef Q_OS_WIN
#include <QWinEventNotifier>
#include <windows.h>
#endif

/**
 * @brief Windows console-based shutdown handler
 *
 * Uses SetConsoleCtrlHandler for Ctrl+C, Ctrl+Break, and console close events
 * to request a graceful application shutdown.
 */
class FRAME_WORKSHARED_EXPORT WindowsShutdownHandler : public ShutdownHandler
{
    Q_OBJECT

public:
    explicit WindowsShutdownHandler(QObject *parent = nullptr);
    ~WindowsShutdownHandler() override;

    bool initialize() override;

private:
#ifdef Q_OS_WIN
    QWinEventNotifier *m_notifier; // reserved for potential future use
    static BOOL WINAPI consoleCtrlHandler(DWORD signal);
    static WindowsShutdownHandler* s_instance;
#endif
};
