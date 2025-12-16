#pragma once

#include "ShutdownHandler.h"
#include <QThread>

#ifdef Q_OS_WIN
#include <QWinEventNotifier>
#include <windows.h>
#else
#include <QSocketNotifier>
#endif

/**
 * @brief Windows console-based shutdown handler
 * 
 * On Windows: Uses SetConsoleCtrlHandler for Ctrl+C, Ctrl+Break, and console close events
 * On Linux: Uses QSocketNotifier to read "quit" or "exit" commands from stdin (fallback)
 * 
 * Provides cross-platform support with stdin command reading as fallback mechanism.
 */
class WindowsShutdownHandler : public ShutdownHandler
{
    Q_OBJECT

public:
    explicit WindowsShutdownHandler(QObject *parent = nullptr);
    ~WindowsShutdownHandler() override;

    bool initialize() override;

Q_SIGNALS:
    void finishedGetLine(const QString &strNewLine);

private Q_SLOTS:
    void on_finishedGetLine(const QString &strNewLine);

private:
#ifdef Q_OS_WIN
    QWinEventNotifier *m_notifier;
    static BOOL WINAPI consoleCtrlHandler(DWORD signal);
    static WindowsShutdownHandler* s_instance;
#else
    QSocketNotifier *m_notifier;
#endif
    QThread m_thread;
};
