# QProcessManager

**Class**: `Daqster::QProcessManager`  
**Inherits**: `QObject`  
**Header**: `src/frame_work/base/src/process/QProcessManager.h`  
**Source**: `src/frame_work/base/src/process/QProcessManager.cpp`

## Overview

Generic process manager за стартиране и управление на child processes. Предоставя базова функционалност за process lifecycle management.

## Purpose

- **Process Lifecycle**: Start, track, terminate processes
- **Handle-based Tracking**: Unique handle за всеки process
- **Graceful Termination**: terminate() с fallback към kill()
- **Event Notifications**: Signals за process state changes
- **Extensibility**: Virtual hooks за customization

## Type Definitions

### ProcessEvent_t
```cpp
enum ProcessEvent_t {
    PROCESS_NA,         // Not available
    PROCESS_STARTED,    // Process started
    PROCESS_STOPPED     // Process stopped
};
```

### ProcessDescriptor_t
```cpp
struct ProcessDescriptor_t {
    QString            Name;        // Executable path/name
    QStringList        Arguments;   // Command line arguments
    QProcess::OpenMode Mode;        // I/O mode
};
```

### ProcessHandle_t
```cpp
typedef uint32_t ProcessHandle_t;  // Unique process identifier
```

## Public API

### Constructor
```cpp
explicit QProcessManager(QObject *parent = nullptr);
```

### Get Process Descriptor
```cpp
bool GetProcessDescriptor(const ProcessHandle_t& handle, 
                         ProcessDescriptor_t& desc) const;
```
Връща descriptor за process по handle. Returns `true` ако process съществува.

## Slots

### StartProcess
```cpp
virtual void StartProcess(const QString& name, 
                         const QStringList& arguments, 
                         QProcess::OpenMode mode = QProcess::ReadWrite);
```

Стартира нов process:
1. Създава `QProcess` обект
2. Извиква `setupProcessEnvironment()` (virtual hook)
3. Стартира процеса
4. Trackва го с уникален handle
5. Emit-ва `ProcessEvent(handle, PROCESS_STARTED)`

**Parameters:**
- `name` - Executable path или име
- `arguments` - Command line arguments
- `mode` - I/O mode (ReadWrite, ReadOnly, WriteOnly, Unbuffered)

### KillAll
```cpp
void KillAll();
```

Terminate-ва всички managed processes:
1. Опитва graceful `terminate()` (10 sec timeout)
2. Ако не отговори, прави force `kill()`
3. Изчиства tracking structures

## Signals

### ProcessEvent
```cpp
void ProcessEvent(const ProcessHandle_t& handle, 
                 const ProcessEvent_t& event);
```

Emit-ва се при промяна на process state:
- `PROCESS_STARTED` - Process започна успешно
- `PROCESS_STOPPED` - Process завърши (exit или kill)

## Virtual Hooks

### setupProcessEnvironment
```cpp
protected:
virtual void setupProcessEnvironment(QProcess* process, 
                                    const QString& name,
                                    const QStringList& arguments);
```

**Override точка** за customization на process environment преди start.

**Default implementation**: Празна (no-op)

**Example override:**
```cpp
void MyManager::setupProcessEnvironment(QProcess* proc, 
                                       const QString& name,
                                       const QStringList& args) {
    // Call base implementation (if needed)
    QProcessManager::setupProcessEnvironment(proc, name, args);
    
    // Add custom environment
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("MY_VAR", "value");
    env.insert("PATH", "/custom/path:" + env.value("PATH"));
    proc->setProcessEnvironment(env);
    
    // Set working directory
    proc->setWorkingDirectory("/custom/workdir");
}
```

## Implementation Details

### Process Tracking
Използва `QMap<ProcessHandle_t, QProcess*>` за tracking на активни процеси.

### Handle Generation
Auto-increment counter за уникални handles.

### Cleanup
- Автоматично cleanup при destructor (`KillAll()`)
- QProcess deletion чрез Qt parent-child ownership
- Signal/slot disconnect при process deletion

### Thread Safety
**Not thread-safe** - трябва да се използва от Qt main thread (Qt event loop).

## Usage Example

### Basic Usage
```cpp
QProcessManager manager;

// Connect to events
connect(&manager, &QProcessManager::ProcessEvent,
        [](ProcessHandle_t handle, ProcessEvent_t event) {
    if (event == QProcessManager::PROCESS_STARTED) {
        qDebug() << "Process" << handle << "started";
    } else if (event == QProcessManager::PROCESS_STOPPED) {
        qDebug() << "Process" << handle << "stopped";
    }
});

// Start a process
QStringList args = {"-v", "--config", "app.conf"};
manager.StartProcess("/usr/bin/myapp", args);

// Later: kill all processes
manager.KillAll();
```

### Custom Subclass
```cpp
class MyProcessManager : public Daqster::QProcessManager {
    Q_OBJECT
public:
    explicit MyProcessManager(QObject* parent = nullptr)
        : QProcessManager(parent) {}

protected:
    void setupProcessEnvironment(QProcess* proc, 
                                const QString& name,
                                const QStringList& args) override {
        // Call base
        QProcessManager::setupProcessEnvironment(proc, name, args);
        
        // Customize environment
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert("MY_APP_CONFIG", "/etc/myapp.conf");
        proc->setProcessEnvironment(env);
    }
};
```

## Error Handling

Process errors се handle-ват чрез Qt signals:
```cpp
connect(process, &QProcess::errorOccurred,
        [](QProcess::ProcessError error) {
    qWarning() << "Process error:" << error;
});
```

## Performance Considerations

- **Lightweight**: Minimal overhead per process
- **Lazy cleanup**: Processes cleaned само при explicit `KillAll()` или destructor
- **No polling**: Event-driven чрез Qt signals

## Limitations

- **Single-threaded**: Must be used from main thread
- **No process groups**: Не управлява process trees (само direct children)
- **No stdio capture**: Subclasses трябва да setup-нат stdio handling

## See Also

- [ApplicationsManager](../apps/ApplicationsManager.md) - Daqster-specific subclass
- [ShutdownHandler](./ShutdownHandler.md) - Graceful shutdown support
- [Framework Overview](./README.md)
