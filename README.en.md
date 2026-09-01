# Daqster
[English](./README.en.md) | [Български](./README.md)

Documentation index: [docs/index.en.md](./docs/index.en.md)

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

**Required versions:**

- **Qt6 (PRIMARY): 6.8.3+** — the code requires Qt 6.8+ APIs: the `QVideoFrame(QImage)` constructor (Qt 6.8+) and `QImage::flipped()` (Qt 6.5+). Ubuntu 24.04's system Qt 6.4.2 is TOO OLD — use aqtinstall or a newer Qt.
- **Qt5 (COMPAT): 5.15.x** — supported for compatibility (Qt 5.15.13 on Ubuntu 24.04, 5.15.2 locally).

**Required Qt modules:** Core, Gui, Widgets, Multimedia, MultimediaWidgets, Charts, Declarative (QuickControls2), Svg

**Linux system dependencies (Ubuntu 24.04):**

- Qt6 via aqtinstall (6.8.3) OR system packages where available
- GStreamer: `libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev` (dev) + `libgstreamer1.0-0 libgstreamer-plugins-base1.0-0 gstreamer1.0-plugins-base gstreamer1.0-plugins-good` (runtime — REQUIRED for the QtMultimedia backend)
- OpenSSL: `libssl-dev`
- ICU: `libicu74` (Ubuntu 24.04) / `libicu70` (Ubuntu 22.04)
- Mesa GL (for offscreen/headless testing): `libgl1-mesa-dri libegl1 libgl1 libglx-mesa0`
- CMake 3.20+, C++17 compiler (GCC/Clang)

**Windows:**

- Qt 6.8.3 (MSVC 2022) via aqtinstall — modules: qtcharts, qtmultimedia (qtdeclarative and qtsvg are in the base package)
- MSVC 2022 + Ninja

**Qt6 (default / preferred):**
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=<qt6-dir> -DDAQSTER_BUILD_TESTS=ON
cmake --build build -j
```

**Qt5 (compat):**
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=<qt5-dir> -DDAQSTER_BUILD_TESTS=ON
cmake --build build -j
```

**With explicit Qt path:**
```bash
cmake -S . -B build \
  -DCMAKE_PREFIX_PATH=/path/to/Qt/6.8.3/gcc_64
cmake --build build -j
```

**With tests (unit + test plugins):**
```bash
cmake -S . -B build -DDAQSTER_BUILD_TESTS=ON -DDAQSTER_BUILD_TEST_PLUGINS=ON
cmake --build build -j
```

**Run the tests (headless):**
```bash
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
```

> **Note:** `DAQSTER_BUILD_TEST_PLUGINS` defaults to OFF — enable it explicitly with `-DDAQSTER_BUILD_TEST_PLUGINS=ON`.

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

> **Note:** Qt6 is detected automatically — `cmake/FindQtVersion.cmake` tries Qt6
> first, then falls back to Qt5. `USE_QT6` is only a FORCE cache variable that
> tells the external libraries (e.g. nodeeditor) which Qt version to build against
> (`CMakeLists.txt:27-31`) — you don't need to set it manually.

For more information see [DeveloperGuide.md](./docs/development/DeveloperGuide.md).

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

For details see [Framework](./docs/Architecture/framework/README.md) and [ApplicationsManager](./docs/Architecture/apps/ApplicationsManager.md).

## Plugin Discovery System

Plugins are searched in the following priority order:

1. Build directory - `./plugins` and `../lib/daqster/plugins` (for debug)
2. Environment variables - `DAQSTER_PLUGIN_DIR` and `DAQSTER_PLUGIN_PATH`
3. User plugins - `~/.local/share/daqster/plugins`
4. System plugins - `/usr/lib/daqster/plugins` and `/usr/local/lib/daqster/plugins`

See [BuildSystemArchitecture](./docs/Architecture/BuildSystemArchitecture.en.md) for details.

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

For complete list see [HowToDebugAppImage](./docs/development/HowToDebugAppImage.md).

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

- [Documentation Index](./docs/index.en.md)
- [Architecture Overview](./docs/Architecture/README.en.md)
- [Development Topics](./docs/development/README.md)
- [Operations Topics](./docs/operations/README.md)
- [Porting Topics](./docs/porting/README.md)
- [Framework Subsystem](./docs/Architecture/framework/README.md)
- [Build System Architecture](./docs/Architecture/BuildSystemArchitecture.en.md)

## Project Structure

- `src/frame_work` - framework core (ShutdownHandler, QProcessManager)
- `src/apps/Daqster` - host application with ApplicationsManager
- `src/plugins` - runtime and test plugins
- `src/plugins/external_libs` - external libraries
- `tools` - build and AppImage scripts
- `docs` - architecture, development, operations, porting and diagrams

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

For detailed guide see [How To Debug AppImage](./docs/development/HowToDebugAppImage.md).
