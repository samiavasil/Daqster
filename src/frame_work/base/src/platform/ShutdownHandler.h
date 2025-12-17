#pragma once

#include <QObject>

/**
 * @brief Abstract base class for handling application shutdown signals
 * 
 * Provides platform-independent interface for graceful application shutdown.
 * Implementations handle OS-specific shutdown mechanisms (signals on Unix, 
 * console events on Windows, or stdin commands).
 */
class ShutdownHandler : public QObject
{
    Q_OBJECT

public:
    explicit ShutdownHandler(QObject *parent = nullptr) : QObject(parent) {}
    virtual ~ShutdownHandler() = default;

    /**
     * @brief Initialize the shutdown handler
     * @return true if initialization successful, false otherwise
     */
    virtual bool initialize() = 0;

Q_SIGNALS:
    /**
     * @brief Emitted when shutdown is requested
     */
    void shutdownRequested();
};
