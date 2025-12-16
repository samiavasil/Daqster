#pragma once

#include "ShutdownHandler.h"
#include <csignal>

/**
 * @brief Unix/Linux signal-based shutdown handler
 * 
 * Handles SIGINT (Ctrl+C) and SIGTERM (kill command) for graceful shutdown.
 * Standard Unix approach using signal handlers.
 */
class UnixShutdownHandler : public ShutdownHandler
{
    Q_OBJECT

public:
    explicit UnixShutdownHandler(QObject *parent = nullptr);
    ~UnixShutdownHandler() override;

    bool initialize() override;

private:
    static void signalHandler(int signal);
    static UnixShutdownHandler* s_instance;
};
