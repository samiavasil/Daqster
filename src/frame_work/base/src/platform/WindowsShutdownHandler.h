#pragma once

#include "ShutdownHandler.h"

#ifdef Q_OS_WIN
#include <QWinEventNotifier>
#include <windows.h>

namespace Daqster {

/**
 * @brief Windows console-based shutdown handler
 *
 * Uses SetConsoleCtrlHandler for Ctrl+C, Ctrl+Break, and console close events
 * to request a graceful application shutdown.
 */
class FRAMEWORK_CORE_EXPORT WindowsShutdownHandler : public ShutdownHandler // skipcq: CXX-W2009
{
    Q_OBJECT

public:
    explicit WindowsShutdownHandler(QObject *parent = nullptr);
    ~WindowsShutdownHandler() override;

    bool initialize() override;

private:
    static BOOL WINAPI consoleCtrlHandler(DWORD signal);
    static WindowsShutdownHandler* s_instance; // skipcq: CXX-W2009
};

} // namespace Daqster

#endif // Q_OS_WIN