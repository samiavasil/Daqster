#include "UnixShutdownHandler.h"

#include <QCoreApplication>
#include <QDebug>
#include <QSocketNotifier>

#include <unistd.h>

UnixShutdownHandler *UnixShutdownHandler::s_instance = nullptr; // skipcq: CXX-W2009
std::array<int, 2> UnixShutdownHandler::s_sigPipe{{-1, -1}}; // skipcq: CXX-W2009

UnixShutdownHandler::UnixShutdownHandler(QObject *parent)
    : ShutdownHandler(parent)
{
}

UnixShutdownHandler::~UnixShutdownHandler()
{
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

bool UnixShutdownHandler::initialize()
{
    // Enforce single instance semantics per process
    if (s_instance != nullptr && s_instance != this) {
        qWarning() << "UnixShutdownHandler: multiple instances are not supported";
        return false;
    }

    s_instance = this;

    // Create self-pipe once
    if (s_sigPipe[0] == -1 && s_sigPipe[1] == -1) {
        if (::pipe(s_sigPipe.data()) != 0) {
            qWarning() << "UnixShutdownHandler: failed to create signal pipe";
            return false;
        }
    }

    // Install signal handlers (async-signal-safe part)
    std::signal(SIGINT, signalHandler);   // Ctrl+C
    std::signal(SIGTERM, signalHandler);  // kill command

    // QSocketNotifier lives in Qt thread and reacts to pipe activity
    auto *notifier = new QSocketNotifier(s_sigPipe[0], QSocketNotifier::Read, this);
    connect(notifier, &QSocketNotifier::activated,
            this, &UnixShutdownHandler::onSignalActivated);

    qDebug() << "Unix shutdown handler initialized (SIGINT, SIGTERM)";
    return true;
}

void UnixShutdownHandler::signalHandler(int signal)
{
    if (signal == SIGINT || signal == SIGTERM) {
        const char ch = 1;
        // async-signal-safe write to self-pipe
        ::write(s_sigPipe[1], &ch, sizeof(ch));
    }
}

void UnixShutdownHandler::onSignalActivated(int fd)
{
    if (fd != s_sigPipe[0]) {
        return;
    }

    // Read a single byte to clear the notification.
    // The pipe is blocking; reading more than once here could block
    // the Qt event loop if no more data is available.
    char ch;
    ::read(s_sigPipe[0], &ch, sizeof(ch));

    qDebug() << "UnixShutdownHandler: shutdown signal received, emitting shutdownRequested()";
    Q_EMIT shutdownRequested();
}
