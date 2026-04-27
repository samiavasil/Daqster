# Daqster Application

**Executable**: `Daqster`  
**Location**: `src/apps/Daqster/`  
**Type**: Qt Widgets Application

## Overview

Главното приложение на проекта. Предоставя UI за зареждане и управление на плъгини, стартиране на процеси и интеграция на всички компоненти.

## Components

### Main Entry Point
**File**: `main.cpp`

Инициализира:
1. Qt Application (`QApplication`)
2. Framework инициализация
3. Shutdown handler (SIGINT/SIGTERM на Unix, Ctrl+C на Windows)
4. `ApplicationsManager` singleton
5. Plugin loading чрез `QPluginManager`
6. Main window UI

### ApplicationsManager
**File**: `ApplicationsManager.{h,cpp}`

Централен мениджър за:
- Process management
- Plugin environment setup
- AppImage support
- Multi-process coordination

Виж [ApplicationsManager.md](./ApplicationsManager.md) за детайли.

### AppToolbar
**File**: `AppToolbar.{h,cpp}`

Toolbar UI компонент за:
- Toolbar бутони и контроли
- Plugin actions integration
- Quick access меню

### Main Window
**File**: `mainwindow.ui`

Qt Designer UI файл с layout на главния прозорец.

## Build

CMake target: `Daqster`

```bash
cd src/apps
mkdir build && cd build
cmake ..
make Daqster
```

Или от root:
```bash
cmake --build . --target Daqster
```

## Startup Sequence

```
1. main()
   ↓
2. QApplication creation
   ↓
3. ShutdownHandler::install()
   ↓
4. ApplicationsManager::Instance()
   ↓
5. QPluginManager setup
   ↓
6. Load plugins from plugins/Daqster/
   ↓
7. Show main window
   ↓
8. Event loop (exec())
```

Виж [startup sequence diagram](../diagrams/startup_sequence.svg) за визуализация.

## Configuration

### Plugin Paths
```
<install_dir>/plugins/Daqster/
```

### Data Directories
```
XDG_CONFIG_HOME: ~/.config/Daqster/
XDG_CACHE_HOME:  ~/.cache/Daqster/
XDG_DATA_HOME:   ~/.local/share/Daqster/
```

### AppImage Mode
Автоматично детектиран чрез `APPIMAGE` environment variable.

## Running

```bash
# Normal mode
./Daqster

# With specific plugin
./Daqster --plugin NodeEditor

# Headless mode (no GUI)
./Daqster --headless

# Debug mode
QT_DEBUG_PLUGINS=1 ./Daqster
```

## Debugging

Виж [HowToDebugAppImage.md](../HowToDebugAppImage.md) за debugging на AppImage версията.

### Enable Plugin Debug
```bash
export QT_DEBUG_PLUGINS=1
./Daqster
```

### GDB
```bash
gdb ./Daqster
(gdb) run
(gdb) bt  # backtrace при crash
```

## Architecture

```
Daqster Application
├── ApplicationsManager (Singleton)
│   ├── QProcessManager (Base)
│   ├── QPluginManager (Plugin system)
│   └── ShutdownHandler (Signal handling)
│
├── UI Layer
│   ├── Main Window
│   ├── AppToolbar
│   └── Plugin Widgets
│
└── Plugins
    ├── NodeEditor
    ├── QtCoinTrader
    └── ... (extensible)
```

## See Also

- [ApplicationsManager API](./ApplicationsManager.md)
- [Framework Overview](../framework/README.md)
- [Plugin Development](../plugins/PluginDevelopment.md)
- [Architecture Diagram](../diagrams/architecture.svg)
