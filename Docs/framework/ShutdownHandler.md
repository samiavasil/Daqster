# ShutdownHandler

**Class**: `ShutdownHandler` (abstract base)  
**Inherits**: `QObject`  
**Header**: `src/frame_work/base/src/platform/ShutdownHandler.h`

## Overview

Platform-independent interface за graceful application shutdown. Handle-ва OS-specific shutdown сигнали и ги превръща в Qt signal.

## Purpose

- **Cross-platform**: Единен интерфейс за Unix и Windows
- **Graceful shutdown**: Позволява cleanup преди exit
- **Qt integration**: Emit-ва Qt signal за shutdown
- **Signal safety**: Safe signal handling on Unix

## Platform Implementations

### UnixShutdownHandler
**Files**: `UnixShutdownHandler.{h,cpp}`

Handle-ва Unix signals:
- `SIGINT` (Ctrl+C)
- `SIGTERM` (kill command)

**Implementation**:
- Signal handler пише в pipe
- Qt socket notifier чете от pipe (async-signal-safe)
- Emit-ва `shutdownRequested()` от main thread

### WindowsShutdownHandler  
**Files**: `WindowsShutdownHandler.{h,cpp}`

Handle-ва Windows console events:
- `CTRL_C_EVENT` (Ctrl+C)
- `CTRL_BREAK_EVENT` (Ctrl+Break)
- `CTRL_CLOSE_EVENT` (Close console window)

**Implementation**:
- Console handler callback
- Emit-ва `shutdownRequested()` (Windows thread-safe)

## Factory Method

```cpp
static std::unique_ptr<ShutdownHandler> create();
```

Автоматично избира правилната имплементация based on platform:
```cpp
#ifdef Q_OS_UNIX
    return std::make_unique<UnixShutdownHandler>();
#elif defined(Q_OS_WIN)
    return std::make_unique<WindowsShutdownHandler>();
#else
    return nullptr; // Unsupported platform
#endif
```

## Public API

### initialize()
```cpp
virtual bool initialize() = 0;
```

Инициализира platform-specific shutdown handling.

**Returns**: `true` при успех, `false` при грешка

**Must be called** преди shutdown може да се catch-не.

## Signals

### shutdownRequested()
```cpp
void shutdownRequested();
```

Emit-ва се когато OS поиска shutdown (SIGINT, SIGTERM, Ctrl+C, etc).

## Usage Pattern

### Basic Usage
```cpp
#include "ShutdownHandler.h"

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    
    // Create platform-specific handler
    auto handler = ShutdownHandler::create();
    if (!handler) {
        qFatal("Shutdown handler not supported on this platform");
        return 1;
    }
    
    // Initialize
    if (!handler->initialize()) {
        qFatal("Failed to initialize shutdown handler");
        return 1;
    }
    
    // Connect to shutdown signal
    QObject::connect(handler.get(), &ShutdownHandler::shutdownRequested,
                    &app, &QCoreApplication::quit);
    
    return app.exec();
}
```

### With Graceful Cleanup
```cpp
int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    
    // Application setup
    MyApp myApp;
    
    // Shutdown handler
    auto handler = ShutdownHandler::create();
    handler->initialize();
    
    QObject::connect(handler.get(), &ShutdownHandler::shutdownRequested,
                    &myApp, &MyApp::gracefulShutdown);
    
    return app.exec();
}

// MyApp class
class MyApp : public QObject {
    Q_OBJECT
public slots:
    void gracefulShutdown() {
        qDebug() << "Shutdown requested, cleaning up...";
        
        // Stop processes
        processManager->KillAll();
        
        // Save state
        saveConfiguration();
        
        // Close connections
        database->close();
        
        qDebug() << "Cleanup complete, exiting";
        QCoreApplication::quit();
    }
};
```

## Implementation Details

### Unix (POSIX Signals)

**Challenge**: Signal handlers мога да извикат само async-signal-safe функции.  
**Solution**: Self-pipe trick

```cpp
// Signal handler (async-signal-safe)
static void signalHandler(int signal) {
    char a = 1;
    ::write(sigPipe[1], &a, sizeof(a));  // Write to pipe
}

// Main thread (Qt event loop)
void UnixShutdownHandler::initialize() {
    // Create pipe
    if (::pipe(sigPipe) != 0) return false;
    
    // Install signal handlers
    struct sigaction sa;
    sa.sa_handler = signalHandler;
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);
    
    // Qt socket notifier watches pipe
    notifier = new QSocketNotifier(sigPipe[0], QSocketNotifier::Read);
    connect(notifier, &QSocketNotifier::activated,
            this, &ShutdownHandler::shutdownRequested);
}
```

### Windows (Console Events)

```cpp
// Console handler callback
BOOL WINAPI consoleHandler(DWORD signal) {
    if (signal == CTRL_C_EVENT || 
        signal == CTRL_BREAK_EVENT ||
        signal == CTRL_CLOSE_EVENT) {
        // Emit signal (Windows allows this)
        emit shutdownRequested();
        return TRUE;  // Handled
    }
    return FALSE;
}

void WindowsShutdownHandler::initialize() {
    SetConsoleCtrlHandler(consoleHandler, TRUE);
}
```

## Thread Safety

### Unix
✅ **Thread-safe**: Pipe + socket notifier pattern е async-signal-safe

### Windows
✅ **Thread-safe**: Windows console handler може да emit Qt signals

## Error Handling

```cpp
auto handler = ShutdownHandler::create();
if (!handler) {
    // Platform not supported
    qWarning() << "Shutdown handler not available";
    // Fallback: no graceful shutdown
}

if (!handler->initialize()) {
    // Initialization failed
    qWarning() << "Failed to install shutdown handler";
    // Continue without graceful shutdown
}
```

## Testing

### Manual Test
```bash
# Start application
./Daqster

# Send SIGINT (Ctrl+C)
^C

# Or send SIGTERM
kill <pid>

# Verify graceful shutdown in logs
```

### Automated Test
```cpp
TEST(ShutdownHandler, SignalHandling) {
    auto handler = ShutdownHandler::create();
    ASSERT_NE(handler, nullptr);
    ASSERT_TRUE(handler->initialize());
    
    QSignalSpy spy(handler.get(), &ShutdownHandler::shutdownRequested);
    
    // Simulate signal
    #ifdef Q_OS_UNIX
    ::kill(::getpid(), SIGINT);
    #endif
    
    ASSERT_EQ(spy.count(), 1);
}
```

## Limitations

- **macOS**: Трябва да се тества (вероятно работи като Unix)
- **Headless**: На некои системи няма console events
- **Multiple signals**: Само един handler per signal type

## See Also

- [QProcessManager](./QProcessManager.md) - Process cleanup при shutdown
- [ApplicationsManager](../apps/ApplicationsManager.md) - Използва ShutdownHandler
- [Framework Overview](./README.md)
