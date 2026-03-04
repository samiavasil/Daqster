#pragma once

#include "ShutdownHandler.h"

#include <array>
#include <csignal>

class QSocketNotifier;

/**
 * @brief Unix/Linux signal-based shutdown handler
 *
 * Uses POSIX signals (SIGINT, SIGTERM) and the self-pipe pattern to
 * safely forward shutdown requests into the Qt event loop.
 *
 * Signal handler (async context) writes to a pipe (async-signal-safe),
 * a QSocketNotifier on the read-end lives in the Qt thread and emits
 * ShutdownHandler::shutdownRequested().
 */
class FRAME_WORKSHARED_EXPORT UnixShutdownHandler : public ShutdownHandler
{
    Q_OBJECT

public:
    explicit UnixShutdownHandler(QObject *parent = nullptr);
    ~UnixShutdownHandler() override;

    bool initialize() override;

private Q_SLOTS:
    void onSignalActivated(int fd);

private:
    static void signalHandler(int signal);

    // One handler per process – used only from the Qt thread
    static UnixShutdownHandler *s_instance;

    // Self-pipe used from signal handler (write) and Qt thread (read)
    static std::array<int, 2> s_sigPipe;
};
