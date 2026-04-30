# Daqster
[English](./README.en.md) | [Български](./README.md)

Documentation index: [Docs/INDEX.en.md](./Docs/INDEX.en.md)

Daqster is a Qt-based platform for modular application development and management. It enables building various types of applications through a plugin architecture with graceful shutdown, process management, and automatic plugin discovery.

Site: https://samiavasil.github.io/Daqster/

## Quick Start

### 1) Clone
```bash
git clone https://github.com/samiavasil/Daqster.git
cd Daqster
git submodule update --init --recursive
```

### 2) Configure and build

**Qt5 (default):**
```bash
cmake -S . -B build -DUSE_QT6=OFF
cmake --build build -j
```

**Qt6:**
```bash
cmake -S . -B build -DUSE_QT6=ON
cmake --build build -j
```

**With explicit Qt path:**
```bash
cmake -S . -B build \
  -DUSE_QT6=OFF \
  -DCMAKE_PREFIX_PATH=/path/to/Qt/5.15.2/gcc_64
cmake --build build -j
```

**Debug Build (recommended for development):**
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
```

**Release Build:**
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

For more information see [DeveloperGuide.md](./Docs/development/DeveloperGuide.md).

### 3) Run
```bash
cd build/bin
./Daqster
```

## Framework Architecture

Daqster uses a modular architecture with three key layers:

- **ShutdownHandler** - graceful shutdown with cross-platform signal handling (Ctrl+C, SIGTERM)
- **QProcessManager** - generic child process management with virtual hooks
- **ApplicationsManager** - Daqster-specific implementation with environment setup and plugin management

For details see [Framework](./Docs/Architecture/framework/README.md) and [ApplicationsManager](./Docs/Architecture/apps/ApplicationsManager.md).

## Plugin Discovery System

Plugins are searched in the following priority order:

1. Build directory - `./plugins` and `../lib/daqster/plugins` (for debug)
2. Environment variables - `DAQSTER_PLUGIN_DIR` and `DAQSTER_PLUGIN_PATH`
3. User plugins - `~/.local/share/daqster/plugins`
4. System plugins - `/usr/lib/daqster/plugins` and `/usr/local/lib/daqster/plugins`

See [BuildSystemArchitecture](./Docs/Architecture/BuildSystemArchitecture.en.md) for details.

## Environment Variables

**Plugin Discovery:**
- `DAQSTER_PLUGIN_DIR` - single directory for plugins
- `DAQSTER_PLUGIN_PATH` - multiple directories separated by `:` (like PATH)

**Qt (for AppImage):**
- `LD_LIBRARY_PATH` - paths to shared libraries
- `QT_PLUGIN_PATH` - paths to Qt plugins
- `QML2_IMPORT_PATH` - paths to QML modules

**XDG Directories (for AppImage):**
- `XDG_CONFIG_HOME` - configuration files (default: `~/.config/daqster`)
- `XDG_DATA_HOME` - user data (default: `~/.local/share/daqster`)
- `XDG_CACHE_HOME` - cache (default: `~/.cache/daqster`)

For complete list see [HowToDebugAppImage](./Docs/development/HowToDebugAppImage.md).

## AppImage

**Local build:**
```bash
./tools/create_appimage.sh
```

**With additional options:**
```bash
./tools/create_appimage.sh --help
```

For details see [tools/create_appimage.sh](./tools/create_appimage.sh).

## Documentation

- [Documentation Index](./Docs/INDEX.en.md)
- [Architecture Overview](./Docs/Architecture/README.en.md)
- [Development Topics](./Docs/development/README.md)
- [Operations Topics](./Docs/operations/README.md)
- [Porting Topics](./Docs/porting/README.md)
- [Framework Subsystem](./Docs/Architecture/framework/README.md)
- [Build System Architecture](./Docs/Architecture/BuildSystemArchitecture.en.md)

## Project Structure

- `src/frame_work` - framework core (ShutdownHandler, QProcessManager)
- `src/apps/Daqster` - host application with ApplicationsManager
- `src/plugins` - runtime and test plugins
- `src/external_libs` - external libraries
- `tools` - build and AppImage scripts
- `Docs` - architecture, development, operations, porting and diagrams

## Debug and Diagnostics

**Useful environment variables:**

- `QT_DEBUG_PLUGINS=1` - Qt plugin debug information
- `QT_LOGGING_RULES="*=true"` - all Qt debug messages
- `DAQSTER_PLUGIN_DIR` - plugin directory path (for testing)
- `DAQSTER_PLUGIN_PATH` - multiple plugin paths

**Examples:**
```bash
# Single plugin directory
DAQSTER_PLUGIN_DIR=/path/to/plugins ./Daqster

# Debug mode
QT_DEBUG_PLUGINS=1 QT_LOGGING_RULES="*=true" ./Daqster

# AppImage with custom paths
LD_LIBRARY_PATH="/custom/lib:$LD_LIBRARY_PATH" ./Daqster-x86_64.AppImage
```

For detailed guide see [How To Debug AppImage](./Docs/development/HowToDebugAppImage.md).
