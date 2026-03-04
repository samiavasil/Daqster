#pragma once

#include "ShutdownHandler.h"

#include <QObject>

#ifdef Q_OS_WIN
#include <QWinEventNotifier>
#include <windows.h>
#else
#include <QSocketNotifier>
#include <cstdio>
#endif

/**
 * @brief Cross-platform stdin-based shutdown handler
 *
 * Listens on standard input and emits shutdownRequested() when the user
 * types "quit" or "exit" followed by Enter.
 *
 * This is orthogonal to OS-specific shutdown mechanisms (Unix signals,
 * Windows console control events) and can be combined with them.
 */
class StdinShutdownHandler : public ShutdownHandler
{
    Q_OBJECT

public:
    explicit StdinShutdownHandler(QObject *parent = nullptr);
    ~StdinShutdownHandler() override;

    bool initialize() override;

private Q_SLOTS:
    void onActivated();
};
