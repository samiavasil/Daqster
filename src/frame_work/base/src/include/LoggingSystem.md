# Daqster Logging System

## Architecture

The Daqster logging system replaces Qt's default message handler with an installable custom handler that enriches log output with timestamps, PID-tagged source identification, and instance IDs. A singleton `LogManager` (`src/frame_work/base/src/LogManager.cpp`) centralizes control of logging categories, output targets (console, file, in-process widget), and per-process identification. Categories are declared using Qt's `Q_DECLARE_LOGGING_CATEGORY`/`Q_LOGGING_CATEGORY` macros and are filterable at runtime — all categories start disabled, and users enable them either programmatically or through the `DebugConsoleWidget` GUI.

## Log Levels

| Level | Qt Type | Tag | Description |
|-------|---------|-----|-------------|
| Debug | QtDebugMsg | DBG | Detailed debug information for developers |
| Info | QtInfoMsg | INF | General informational messages about normal operation |
| Warning | QtWarningMsg | WRN | Potentially harmful situations that are not errors |
| Critical | QtCriticalMsg | CRT | Error events from which recovery is possible |
| Fatal | QtFatalMsg | FTL | Unrecoverable errors; `abort()` is called immediately after logging |

Levels are compared by integer rank defined in the `Daqster::LogLevel` enum: `Debug = 0`, `Info = 1`, `Warning = 2`, `Critical = 3`, `Fatal = 4`. Messages at or above the configured threshold are shown on the console.

## Output Format

```
[2026-07-28 12:34:56.789] [WRN] [PID:12345:inst_id] Some message
```

| Field | Source | Description |
|-------|--------|-------------|
| `2026-07-28 12:34:56.789` | `QDateTime::currentDateTime()` | Timestamp with millisecond precision in ISO-8601 style |
| `WRN` | `QtMsgType` → tag mapping | Four-character level tag (DBG/INF/WRN/CRT/FTL) |
| `PID:12345` | `getpid()` / `GetCurrentProcessId()` | OS process ID, tagged for multi-process scenarios |
| `:inst_id` | `LogManager::setInstanceId()` | Optional instance identifier (e.g., child process UUID) |
| `Some message` | The logged text | The actual message passed to `qCDebug`/`qCInfo`/etc. |

## Consumers

The custom message handler (`daqsterMessageHandler`) performs an early-exit check before doing any work: if no consumer exists and the message is not `Fatal`, it returns immediately — no formatting, no allocation, no waste.

A consumer exists when **any** of the following is true:
- A log file is open and writable
- Console output is enabled
- The `LogManager` instance is alive (the `DebugConsoleWidget`'s `messageLogged` signal is connected via the instance)

Three consumer types forward formatted log lines:

### 1. Console Output

Controlled by:
- CLI flag `--log-console-enabled 0|1` parsed in `main.cpp`
- Programmatic call `LogManager::setConsoleEnabled(bool)`
- `DebugConsoleWidget` "Enable Console Logging" checkbox

When enabled, messages at or above `consoleLogLevel()` threshold are written to the appropriate stdio stream:
- `stderr` for Warning, Critical, Fatal
- `stdout` for Debug, Info

### 2. File Output

Controlled by:
- `LogManager::setLogFile(path)` — opens the path in append mode, creating parent directories if needed
- `DebugConsoleWidget` "Log to file" checkbox + Browse button
- Passing an empty string (`setLogFile("")`) stops file logging

The log file receives every formatted line regardless of level (the console threshold does not apply to file output). The `QFile*` is stored as a file-scope static (`s_logFile`) so the message handler can access it directly without needing the `LogManager` instance.

### 3. DebugConsoleWidget

An in-process `QWidget` subclass that connects to the `LogManager::messageLogged` signal. The widget provides checkboxes for category enable/disable and controls for output configuration. It receives all formatted log lines via the signal and can display them in a GUI console panel (the display mechanism is external to the widget itself).

### Priority / Fallback After LogManager Destruction

When `LogManager::shutdown()` is called:
1. The log file is closed and `s_logFile` is set to `nullptr`
2. `s_consoleEnabled` is set to `false`
3. `s_logManagerInstance` (the file-scope alias) is set to `nullptr` — this stops widget signal emissions

After destruction, only Warning, Critical, and Fatal messages are written directly to `stderr` as a fallback (line 89–93 of `LogManager.cpp`). All category filtering stops working because `QLoggingCategory` rules are reset — if a message reaches `qInstallMessageHandler`, it is shown.

## Configuration

### Command Line

The `main.cpp` entry point (`src/apps/Daqster/main.cpp`) parses the following options **after** `QApplication` is constructed:

| Option | Type | Purpose | Example |
|--------|------|---------|---------|
| `--instance-id <id>` | String | Process identifier tag (used internally by child processes) | `--instance-id abcd1234` |
| `--log-console-enabled 0|1` | 0 or 1 | Enable/disable console output | `--log-console-enabled 1` |
| `--log-level <level>` | String | Minimum console log level (Debug/Info/Warning/Critical/Fatal) | `--log-level Debug` |

These are primarily intended for child process forwarding (see "Child Process Log Forwarding" below). The `--instance-id` option is also available as a public CLI flag for the main Daqster process.

### DebugConsole Widget

The `DebugConsoleWidget` (`src/frame_work/base/src/gui/DebugConsoleWidget.cpp`) is a self-contained settings panel with the following layout:

```
┌─────────────────────────────────────────────┐
│ ☐ Enable Debug Logging (all categories)     │  ← Master toggle
│                                             │
│ ┌─ Framework ──────────────────────────┐    │
│ │ ☐ daqster.framework                  │    │
│ │ ☐ daqster.framework.registry         │    │
│ │ ☐ daqster.framework.discovery        │    │
│ │ ☐ daqster.framework.persistence      │    │
│ │ ☐ daqster.framework.shutdown         │    │
│ │ ☐ daqster.framework.process          │    │
│ └──────────────────────────────────────┘    │
│ ┌─ Application ────────────────────────┐    │
│ │ ☐ daqster.app                        │    │
│ └──────────────────────────────────────┘    │
│ ┌─ Plugins ────────────────────────────┐    │
│ │ ☐ daqster.plugin.nodeeditor          │    │
│ │ ☐ daqster.plugin.demo                │    │
│ │ ☐ daqster.plugin.aistudio            │    │
│ │ ☐ daqster.plugin.aistudio.llama      │    │
│ │ ☐ daqster.plugin.cointrader          │    │
│ └──────────────────────────────────────┘    │
│                                             │
│ ┌─ Console Output ─────────────────────┐    │
│ │ Enable Console Logging:          [☐] │    │
│ │ Minimum Log Level: [Warning ▾    ]   │    │
│ └──────────────────────────────────────┘    │
│                                             │
│ ┌─ File Output ───────────────────────┐    │
│ │ ☐ Log to file: [______________] [Browse] │
│ └──────────────────────────────────────┘    │
│                                             │
│ [Reset to Defaults]                         │
└─────────────────────────────────────────────┘
```

**Master toggle**: Enables or disables all categories at once. Calls `LogManager::enableAll()` which sets Qt's filter rule to `"*.debug=true"`, or `LogManager::disableAll()` which clears the filter rules.

**Per-category checkboxes**: Grouped into Framework (6 categories), Application (1 category), and Plugins (5 categories). Each checkbox calls `LogManager::enableCategory()` or `LogManager::disableCategory()`, which maintains an internal list of enabled categories and applies them as Qt filter rules (`category=true` lines joined by newline).

**Enable Console Logging checkbox**: Toggles `LogManager::setConsoleEnabled()`. When unchecked, the Minimum Log Level dropdown is disabled.

**Minimum Log Level dropdown**: Defaults to Warning. Controls the `consoleLogLevel` threshold — messages below this level are suppressed from console output. The dropdown is disabled until console logging is enabled.

**Log to file checkbox + path + Browse**: When checked and a non-empty path is provided, calls `LogManager::setLogFile()`. The Browse button opens a `QFileDialog::getSaveFileName()` dialog filtered to `*.log`. Unchecking calls `setLogFile("")` to stop file logging.

**Reset to Defaults button**: Resets everything to default state — disables all categories, disables console output, sets log level to Warning, stops file logging.

### Programmatic API

```cpp
// Get singleton
LogManager *lm = LogManager::instance();

// Enable/disable output
lm->setConsoleEnabled(true);
lm->setConsoleLogLevel(LogLevel::Debug);

// Category control
lm->enableCategory("daqster.app");          // Enable one category
lm->enableAll();                             // Enable all
lm->disableAll();                            // Disable all

// File logging
lm->setLogFile("/path/to/daqster.log");
lm->setLogFile("");  // Stop file logging

// Instance identification
lm->setInstanceId("my-instance");

// Query state
bool enabled = lm->isCategoryEnabled("daqster.app");
QString rules = lm->filterRules();           // Current Qt filter rules
QString filePath = lm->logFilePath();
LogLevel level = lm->consoleLogLevel();
QString levelName = lm->consoleLogLevelName();

// Signals (QObject, connect to these)
lm->messageLogged(formattedString, msgType); // Emitted for every log line
lm->filterRulesChanged(rules);
lm->logFileChanged(path);
lm->consoleEnabledChanged(bool);
lm->consoleLogLevelChanged(level);
```

## Usage in Code

### 1. Define a Category

In a header file:
```cpp
Q_DECLARE_LOGGING_CATEGORY(lcMyCategory)
```

In exactly one source file (the corresponding `.cpp`):
```cpp
Q_LOGGING_CATEGORY(lcMyCategory, "daqster.plugin.myplugin")
```

The string name follows a dotted hierarchical convention. Existing categories use three top-level namespaces:
- `daqster.framework.*` — framework core (6 categories)
- `daqster.app` — application
- `daqster.plugin.*` — plugin subsystems (5 categories)

### 2. Use Category

```cpp
qCDebug(lcMyCategory) << "Debug message";
qCInfo(lcMyCategory) << "Info message";
qCWarning(lcMyCategory) << "Warning message";
qCCritical(lcMyCategory) << "Critical message";
```

Qt's `QLoggingCategory` infrastructure evaluates whether the category is enabled before the message handler is invoked. If the category is disabled, the message is discarded before any formatting occurs — zero overhead for disabled categories.

### Differences from old DEBUG macros

The legacy `debug.h` header (`src/frame_work/base/src/include/debug.h`) defines compile-time macros:

```cpp
#define DEBUG   QDebug(QtDebugMsg) << "DBG:   " << __FILE__ << " Line:" << __LINE__ << ": "
#define WARNING QDebug(QtWarningMsg) << "Warn:  " << __FILE__ << " Line:" << __LINE__ << ": "
```

Key differences:

| Aspect | Old `DEBUG()` macros | New `qCDebug()` system |
|--------|---------------------|------------------------|
| **Filtering** | Compile-time (`#ifdef ENABLE_DUMP`) | Runtime (category enable/disable) |
| **Granularity** | Global — all or nothing | Per-category, hierarchical |
| **Format** | Always includes `__FILE__` and `__LINE__` | Clean timestamped format with PID |
| **Output routing** | Always to Qt's default handler | Custom handler with file/console/widget routing |
| **Overhead when off** | `while(false) QNoDebug()` — minimal | Qt skips message creation entirely for disabled categories |

The old macros are still available for legacy code but new code should prefer the `qCDebug`/`qCInfo`/`qCWarning`/`qCCritical` family for runtime-filterable logging.

## Child Process Log Forwarding

When the Daqster application spawns a child process via `QProcessManager::StartProcess()` (`src/frame_work/base/src/process/QProcessManager.cpp`), the logging system is designed to forward log output from child to parent seamlessly.

### The Chain

1. **Parent injects logging arguments**: `QProcessManager::StartProcess()` generates a short UUID as the child's instance ID, then appends `--instance-id`, `--log-console-enabled`, and `--log-level` arguments to the child's command line:

    ```cpp
    QString instanceId = QUuid::createUuid().toString(QUuid::Id128).left(8);
    args << "--instance-id" << instanceId;
    args << "--log-console-enabled" << (parentLog->isConsoleEnabled() ? "1" : "0");
    args << "--log-level" << parentLog->consoleLogLevelName();
    ```

2. **Child configures its LogManager**: The child process's `main.cpp` parses these arguments and applies them to its own `LogManager` instance (see "Command Line" section above). The child's `LogManager::initialize()` installs the custom message handler, so child output uses the same `[timestamp] [LEVEL] [PID:inst_id]` format.

3. **Child writes to stderr**: Because `--log-console-enabled 1` is forwarded, the child writes its formatted log lines to stderr.

4. **Parent captures stderr**: `QProcessManager` connects `QProcess::readyReadStandardError`:

    ```cpp
    connect(newProc, &QProcess::readyReadStandardError, this, [newProc, instanceId]() {
        QByteArray output = newProc->readAllStandardError();
        if (!output.isEmpty()) {
            QString text = QString::fromLocal8Bit(output).trimmed();
            if (text.startsWith('[')) {
                // Already formatted by child's LogManager — write directly to stderr
                fprintf(stderr, "%s\n", text.toUtf8().constData());
                fflush(stderr);
            } else {
                // Raw output — wrap with child: tag
                qCInfo(lcProcess) << "[child:" << instanceId << "]" << text;
            }
        }
    });
    ```

5. **Pass-through vs. wrap**: If the child output starts with `[` (already formatted by Daqster's handler), it is written directly to `stderr` via `fprintf`. If it is raw output (e.g., from a non-Daqster child), it is routed through `qCInfo(lcProcess)` with a `[child:<instanceId>]` prefix tag, ensuring it goes through the parent's own logging pipeline (file, widget, and console).

The same logic applies for stdout via `readyReadStandardOutput` — formatted output is passed directly to `stdout`, raw output is tagged with `[child:<instanceId>:out]`.

## Default State

| Setting | Default | Controlled by |
|---------|---------|---------------|
| Console output | DISABLED | `s_consoleEnabled = false` |
| Console log level | Warning | `Private::logLevel = LogLevel::Warning` |
| File output | DISABLED | `s_logFile = nullptr` |
| All categories | DISABLED | `initialize()` installs handler but does not set any filter rules |
| Message handler | Custom (Daqster format) | `qInstallMessageHandler(daqsterMessageHandler)` in `initialize()` |

The design philosophy: **silent by default**. No logging output is produced until the user or an administrator explicitly enables it. This avoids noise in production and keeps performance overhead negligible when debugging is not needed.

## Shutdown Behavior

The shutdown sequence in `main.cpp` is:

```
QPluginManager::ShutdownPluginManager()  ← Step 1: plugin cleanup, child process termination
LogManager::shutdown()                   ← Step 2: close log file, reset console, null widget pointer
```

### Step 1 — `ShutdownPluginManager()`
This triggers plugin lifecycle cleanup, including `QProcessManager::KillAll()` which terminates child processes. Child termination may produce final log messages as processes exit.

### Step 2 — `LogManager::shutdown()`
The `shutdown()` method:
1. Closes the log file and deletes `s_logFile` (sets to `nullptr`)
2. Sets `s_consoleEnabled = false`
3. Sets `s_logManagerInstance = nullptr` (the file-scope alias)

After this point:
- File output stops (file is closed)
- Console output is disabled
- The `messageLogged` signal is no longer emitted (widget is disconnected)
- However, `qInstallMessageHandler(daqsterMessageHandler)` is **not** reset — the custom handler remains active

### Destructors and the Fallback
The `LogManager` destructor calls `shutdown()` then deletes the private data. After the destructor completes:
- `s_logFile` is `nullptr`
- `s_consoleEnabled` is `false`
- `s_logManagerInstance` is `nullptr`

In the message handler, the fallback logic (lines 89–93) ensures that Warning, Critical, and Fatal messages are still written directly to `stderr` even without any consumer. This means destructor messages from other Qt objects still use the Daqster format until the process exits. Messages below Warning level are silently dropped.

The original Qt message handler is never restored — there is no `qInstallMessageHandler(0)` call. This is intentional: the Daqster format is maintained for all remaining log output during process teardown.

### Post-Shutdown Consumer Check Logic

```
hasConsumer = (type == QtFatalMsg)         // Always true for Fatal
           || (s_logFile && isOpen())      // false — file closed
           || s_consoleEnabled             // false — reset
           || s_logManagerInstance;        // false — nulled

Result: hasConsumer = false (unless Fatal)
→ Early return for Debug/Info/Warning/Critical
→ Fallback stderr write only for Warning/Critical/Fatal
```
