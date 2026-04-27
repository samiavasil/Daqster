# Daqster Framework

Framework подсистемата предоставя основните building blocks за приложения и плъгини.

## Структура

```
src/frame_work/
├── CMakeLists.txt
├── base/src/
│   ├── platform/              # Platform abstraction
│   │   ├── ShutdownHandler.h
│   │   ├── UnixShutdownHandler.{h,cpp}
│   │   └── WindowsShutdownHandler.{h,cpp}
│   │
│   ├── process/               # Process management
│   │   ├── QProcessManager.{h,cpp}
│   │   └── Process descriptors
│   │
│   ├── QPluginManager.cpp     # Plugin loading system
│   ├── QPluginInterface.cpp   # Plugin interface
│   ├── QPluginListView.{h,cpp} # Plugin UI
│   └── ...
│
├── icons/                     # Framework resources
└── framework_resources.qrc
```

## Core Components

### [QProcessManager](./QProcessManager.md)
Управление на външни процеси:
- Process lifecycle (start/stop/restart)
- Environment setup
- Signal/slot интеграция
- Virtual hooks за customization

### [ShutdownHandler](./ShutdownHandler.md)
Platform-independent graceful shutdown:
- Unix: SIGINT, SIGTERM handling
- Windows: Ctrl+C, Ctrl+Break, Close console
- Qt integration чрез signals

### [QPluginManager](./QPluginManager.md)
Plugin система:
- Dynamic plugin loading
- Dependency management
- Plugin lifecycle
- Interface negotiation

### [Plugin System](./PluginSystem.md)
Общ преглед как работи plugin системата.

## Design Patterns

### Singleton
- `QPluginManager` - global plugin registry
- Applications (напр. `ApplicationsManager`)

### Template Method
- `QProcessManager::setupProcessEnvironment()` - virtual hook
- `ShutdownHandler::handleShutdown()` - platform-specific

### Factory
- Plugin instantiation via `QPluginLoader`

### Observer
- Signals/Slots за process events, plugin events

## Key Features

### 1. Cross-Platform Support
- Unix/Linux (signals)
- Windows (console events)
- macOS compatibility

### 2. Process Management
- Start/stop processes
- Environment inheritance
- Custom environment setup
- Process communication (stdin/stdout/stderr)

### 3. Plugin Architecture
- Hot-loading plugins
- Dependency resolution
- Version compatibility
- Plugin metadata

### 4. Resource Management
- Automatic cleanup
- RAII idioms
- Qt parent-child ownership

## Building

```bash
cd src/frame_work
mkdir build && cd build
cmake ..
make
```

Framework се билдва като library (`libframe_work.so` или `.dll`).

## Usage in Applications

```cpp
#include "QProcessManager.h"
#include "ShutdownHandler.h"

class MyApp : public Daqster::QProcessManager {
    Q_OBJECT
public:
    MyApp() {
        // Install shutdown handler
        auto handler = Daqster::ShutdownHandler::create();
        connect(handler, &ShutdownHandler::shutdownRequested,
                this, &MyApp::gracefulShutdown);
        handler->install();
    }
    
protected:
    void setupProcessEnvironment(QProcess* proc, 
                                const QString& name,
                                const QStringList& args) override {
        // Custom environment setup
        QProcessManager::setupProcessEnvironment(proc, name, args);
        proc->setEnvironment({"MY_VAR=value"});
    }
};
```

## Threading Model

- **Main thread**: Qt event loop, UI
- **Process management**: QProcess использва separate threads за I/O
- **Signals**: Автоматично marshalled към main thread

## Error Handling

- Qt signals за errors (`errorOccurred()`)
- Process exit codes
- Exception-safe (RAII cleanup)

## Testing

Виж `src/plugins/tests/` за unit tests на framework компонентите.

## Performance

- Lazy initialization
- Copy-on-write (Qt containers)
- Minimal overhead за process spawning
- Efficient signal/slot connections

## See Also

- [QProcessManager API](./QProcessManager.md)
- [ShutdownHandler API](./ShutdownHandler.md)
- [QPluginManager API](./QPluginManager.md)
- [Plugin System Overview](./PluginSystem.md)
- [Applications](../apps/README.md) - Използване на framework
- [Architecture Diagram](../diagrams/architecture.svg)
