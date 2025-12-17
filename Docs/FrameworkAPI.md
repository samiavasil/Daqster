# Framework API Reference

This document describes the public API and usage patterns for the framework core components.

See also: [Architecture](./Architecture.md) and [Developer Guide](./DeveloperGuide.md).

## Overview

This document covers the following core classes and interfaces:

- `Daqster::QProcessManager` — generic process manager (see API below)
- `ShutdownHandler` and platform implementations — graceful shutdown abstraction
- `QPluginManager` and plugin-related interfaces (brief)

Each section contains a short API summary and example usage.

---

## QProcessManager (summary)

Purpose: manage child processes launched by applications, provide graceful
termination and process lifecycle events.

Key types and members:

- `typedef enum ProcessEvent_t { PROCESS_NA, PROCESS_STARTED, PROCESS_STOPPED }`
- `typedef struct ProcessDescriptor_t { QString Name; QStringList Arguments; QProcess::OpenMode Mode; }`
- `typedef uint32_t ProcessHandle_t`
- `explicit QProcessManager(QObject *parent = nullptr)`
- `virtual ~QProcessManager()`
- `virtual void StartProcess(const QString& name, const QStringList& arguments, QProcess::OpenMode mode = QProcess::ReadWrite)`
- `void KillAll()` — gracefully terminate all processes then force-kill after timeout
- `bool GetProcessDescriptor(const ProcessHandle_t& handle, ProcessDescriptor_t& desc) const`
- `signals: void ProcessEvent(const ProcessHandle_t& handle, const ProcessEvent_t& event)`
- `protected slots: virtual void OnProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)` — overridable cleanup hook
- `protected: virtual void setupProcessEnvironment(QProcess* process, const QString& name, const QStringList& args)` — override to customize environment
- `protected: virtual void onAllProcessesFinished()` — override to react when last process stops

Usage notes:

- Subclass `QProcessManager` in your application to customize environment
  and behavior. Example: `ApplicationsManager` in `src/apps/Daqster`.
- Connect to `ProcessEvent` to observe state changes. Use handle values to
  query descriptors with `GetProcessDescriptor()`.

---

## ShutdownHandler (summary)

Purpose: provide a cross-platform way to request graceful application shutdown
from OS events (SIGINT/SIGTERM, Ctrl+C, Windows console events).

Core API:

- `class ShutdownHandler : public QObject` (abstract)
- `virtual void initialize() = 0` — start listening for OS signals/events
- `signals: void shutdownRequested()` — emitted when shutdown is requested

Platform implementations:

- `UnixShutdownHandler` — uses `std::signal` handlers and a Qt wakeup
  mechanism to safely emit `shutdownRequested()` on the Qt main thread.
- `WindowsShutdownHandler` — uses `SetConsoleCtrlHandler` and stdin fallback

Usage:

- Create the appropriate handler instance early in `main()` and connect
  `shutdownRequested()` to your cleanup logic (for example `QProcessManager::KillAll`).

---

## QPluginManager (brief)

`QPluginManager` lives in the framework and provides plugin discovery and GUI
integration. See source in `src/frame_work/base/src/` for more details.

Key points:

- It exposes APIs for scanning `DAQSTER_PLUGIN_DIR`, `DAQSTER_PLUGIN_PATH` and
  user/system plugin directories.
- It provides a GUI `QPluginManagerGui` used by the host app.

---

## Examples

### Starting a process (pseudo-code)

```cpp
class MyManager : public Daqster::QProcessManager {
  void setupProcessEnvironment(QProcess* p, const QString& name, const QStringList& args) override {
    // set env
    p->setProcessEnvironment(QProcessEnvironment::systemEnvironment());
  }

  void onAllProcessesFinished() override {
    qApp->quit();
  }
};

MyManager mgr;
mgr.StartProcess("/usr/bin/mytool", {"--arg"});
connect(&mgr, &Daqster::QProcessManager::ProcessEvent, [](auto h, auto ev){ /* handle */ });
```

---

## Next steps / planned docs

- A full Doxygen-generated API reference is planned and will be published
  under `Docs/api/` (future).

For deeper architecture and developer guidance see [Architecture](./Architecture.md)
and [Developer Guide](./DeveloperGuide.md).
