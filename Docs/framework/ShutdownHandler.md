# ShutdownHandler

**Class**: `ShutdownHandler` (abstract base)  
**Inherits**: `QObject`  
**Header**: `src/frame_work/base/src/platform/ShutdownHandler.h`

## Overview

Platform-independent interface за graceful application shutdown. Handle-ва OS-specific shutdown сигнали и ги превръща в Qt signal.

### Basic Usage (generic)
- `CTRL_C_EVENT` (Ctrl+C)
- `CTRL_BREAK_EVENT` (Ctrl+Break)
- `CTRL_CLOSE_EVENT` (Close console window)

**Implementation**:
- Регистрира `SetConsoleCtrlHandler(consoleCtrlHandler, TRUE)`
- В `consoleCtrlHandler(DWORD signal)` избира човекочитаемо име на event-а
- Използва `QMetaObject::invokeMethod(..., "shutdownRequested", Qt::QueuedConnection)`
    към активния `WindowsShutdownHandler`, за да emit-не сигнала в Qt нишката

### StdinShutdownHandler  
**Files**: `StdinShutdownHandler.{h,cpp}`

Cross-platform handler за команден ред, който слуша standard input за текстови
команди и ги преобразува в `shutdownRequested()` събития.

Handle-ва stdin команди:
- `quit`
- `exit`

**Implementation**:
- На Windows: `QWinEventNotifier` върху `GetStdHandle(STD_INPUT_HANDLE)`
- На Unix: `QSocketNotifier` върху `fileno(stdin)`
- В `onActivated()` чете един ред с `std::getline(std::cin, ...)`, trim-ва до `QString`
- При `"quit"` или `"exit"` (case-insensitive) emit-ва `shutdownRequested()`

> Забележка: текущият Daqster код създава handler-ите директно (`new UnixShutdownHandler`,
> `new WindowsShutdownHandler`, `new StdinShutdownHandler`) в `main.cpp`, вместо да ползва
> обща фабрика. По желание може да се изгради фабричен метод върху този интерфейс.

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

### Usage in Daqster (apps/Daqster/main.cpp)

Daqster комбинира **OS-specific** и **stdin-based** shutdown handler-и:

```cpp
QApplication a(argc, argv);

// 1) OS-specific handler (signals / console events)
#ifdef Q_OS_WIN
    auto *shutdownHandler = new WindowsShutdownHandler(&a);
#else
    auto *shutdownHandler = new UnixShutdownHandler(&a);
#endif

    if (!shutdownHandler->initialize()) {
        qWarning() << "Failed to initialize OS shutdown handler";
    }

// 2) Stdin-based handler (quit/exit from terminal)
    auto *stdinHandler = new StdinShutdownHandler(&a);
    if (!stdinHandler->initialize()) {
        qWarning() << "Failed to initialize stdin shutdown handler";
    }

// 3) Общ shutdown flow
    auto shutdownLambda = [&a]() {
        ApplicationsManager::Instance().KillAll();
        a.quit();
    };

    QObject::connect(shutdownHandler, &ShutdownHandler::shutdownRequested,
                                     &a, shutdownLambda);
    QObject::connect(stdinHandler, &ShutdownHandler::shutdownRequested,
                                     &a, shutdownLambda);
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

### Unix (POSIX Signals) – self-pipe в реалния код

Основната идея е същата като в примерния self-pipe по-горе, но реализирана с
`std::signal` и статични полета в `UnixShutdownHandler`:

```cpp
// UnixShutdownHandler (съкратен пример)
UnixShutdownHandler *UnixShutdownHandler::s_instance = nullptr;
int UnixShutdownHandler::s_sigPipe[2] = {-1, -1};

bool UnixShutdownHandler::initialize()
{
    if (s_instance != nullptr && s_instance != this)
        return false; // само една инстанция на процес

    s_instance = this;

    if (s_sigPipe[0] == -1 && s_sigPipe[1] == -1) {
        if (::pipe(s_sigPipe) != 0)
            return false;
    }

    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    auto *notifier = new QSocketNotifier(s_sigPipe[0], QSocketNotifier::Read, this);
    connect(notifier, &QSocketNotifier::activated,
            this, &UnixShutdownHandler::onSignalActivated);

    return true;
}

void UnixShutdownHandler::signalHandler(int signal)
{
    if (signal == SIGINT || signal == SIGTERM) {
        const char ch = 1;
        ::write(s_sigPipe[1], &ch, sizeof(ch)); // async-signal-safe
    }
}

void UnixShutdownHandler::onSignalActivated(int fd)
{
    if (fd != s_sigPipe[0])
        return;

    char ch;
    ::read(s_sigPipe[0], &ch, sizeof(ch)); // четем един байт, без да блокираме

    Q_EMIT shutdownRequested();
}
```

### Windows (Console Events) – реален код

```cpp
bool WindowsShutdownHandler::initialize()
{
#ifdef Q_OS_WIN
    if (!SetConsoleCtrlHandler(consoleCtrlHandler, TRUE)) {
        return false;
    }
    return true;
#else
    return true; // no-op на други платформи
#endif
}

BOOL WINAPI WindowsShutdownHandler::consoleCtrlHandler(DWORD signal)
{
    if (s_instance) {
        // логване на човеко-четимото име на event-а (Ctrl+C, Close, ...)
        QMetaObject::invokeMethod(s_instance, "shutdownRequested",
                                  Qt::QueuedConnection);
        return TRUE;
    }
    return FALSE;
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
