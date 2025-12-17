# Daqster 
[English](./README.en.md) | [Български](./README.md)

Daqster is a Qt5-based framework for creating and loading plugins, with a host application and sample plugins.

Site: https://samiavasil.github.io/Daqster/

## Quick Start (CMake)

### 1) Clone
```bash
git clone https://github.com/samiavasil/Daqster.git
cd Daqster
git submodule update --init --recursive
```

### 2) Configure & Build

#### Qt Version Selection
```bash
# For Qt5 (default)
cmake -S . -B build -DUSE_QT6=OFF

# For Qt6
cmake -S . -B build -DUSE_QT6=ON

# Auto-detection from CMAKE_PREFIX_PATH
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/qt5
```

#### Debug Build (recommended for development)
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
```

#### Release Build (for production)
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

### 3) Run from build
```bash
cd build/bin
./Daqster
```

### 4) Create AppImage (locally)

#### Using the unified script
```bash
# Local mode (default)
./tools/create_appimage.sh

# With custom parameters
./tools/create_appimage.sh --qt-dir /path/to/qt --source-dir /path/to/build

# Help
./tools/create_appimage.sh --help
```

#### Manual AppImage creation
```bash
# 1. Install to staging directory
cmake --install build --prefix ./stage

# 2. Create AppImage
./tools/create_appimage.sh --mode ci --source-dir ./stage
```

**Result:** `Daqster-x86_64.AppImage` in project root directory

## Install & Run

### Installation
```bash
# Install to custom directory
cmake --install build --prefix ./install_dir

# Or to system directory
sudo cmake --install build --prefix /usr/local
```

### Running
```bash
# From install directory
./install_dir/bin/Daqster

# Or from build directory (for development)
cd build/bin
./Daqster
```

### Framework Architecture

Daqster uses a modular framework architecture that allows creating different types of applications:

#### Platform Abstraction Layer
- **ShutdownHandler** - base class for graceful shutdown:
  - Cross-platform signal handling (Ctrl+C, SIGTERM)
  - Virtual interface for easy extension
  - Unix: SIGINT/SIGTERM signal handlers
  - Windows: Console event handlers + stdin fallback

#### Process Management Layer
- **QProcessManager** - generic base class for managing child processes:
  - Handle-based process tracking
  - Graceful terminate (10s timeout) + force kill fallback
  - Virtual hooks for customization:
    - `setupProcessEnvironment()` - custom environment setup
    - `onAllProcessesFinished()` - cleanup when all processes finish
  - ProcessEvent signals for state changes

#### Application Layer
- **ApplicationsManager** - Daqster-specific implementation:
  - Inherits from `QProcessManager`
  - Backward compatibility (AppHndl_t, AppEvent_t, AppDescriptor_t)
  - Daqster environment setup:
    - AppImage detection and path configuration
    - DAQSTER_PLUGIN_* environment variables
    - XDG directories (config, data, cache)
  - Headless mode support
  - Signal forwarding to GUI components

### Plugin Discovery System

Plugins are searched in the following order (by priority):

1. **Build directory** - `./plugins` and `../lib/daqster/plugins` (for debug)
2. **Environment variables** - `DAQSTER_PLUGIN_DIR` and `DAQSTER_PLUGIN_PATH`
3. **User plugins** - `~/.local/share/daqster/plugins`
4. **System plugins** - `/usr/lib/daqster/plugins` and `/usr/local/lib/daqster/plugins`

### Environment Variables

#### Plugin Discovery
- `DAQSTER_PLUGIN_DIR` - sets a single directory for plugins
- `DAQSTER_PLUGIN_PATH` - sets multiple directories separated by `:` (like PATH)

#### Qt Environment (for AppImage)
- `LD_LIBRARY_PATH` - paths to shared libraries
- `QML2_IMPORT_PATH` - paths to QML modules
- `QT_PLUGIN_PATH` - paths to Qt plugins
- `QT_QPA_PLATFORM_PLUGIN_PATH` - paths to platform plugins

#### XDG Directories (for AppImage)
- `XDG_CONFIG_HOME` - config files (default: `~/.config/daqster`)
- `XDG_DATA_HOME` - data (default: `~/.local/share/daqster`)
- `XDG_CACHE_HOME` - cache (default: `~/.cache/daqster`)

#### Debug Environment
- `QT_DEBUG_PLUGINS=1` - enables debug info for Qt plugins
- `QT_LOGGING_RULES="*=true"` - enables all Qt debug messages
- `DAQSTER_DEBUG=1` - enables Daqster-specific debug messages

**Usage examples:**
```bash
# Single directory for plugins
DAQSTER_PLUGIN_DIR=/path/to/plugins ./Daqster

# Multiple directories for plugins
DAQSTER_PLUGIN_PATH="/path1:/path2:/path3" ./Daqster

# Debug mode
QT_DEBUG_PLUGINS=1 QT_LOGGING_RULES="*=true" ./Daqster

# AppImage with custom paths
LD_LIBRARY_PATH="/custom/lib:$LD_LIBRARY_PATH" ./Daqster-x86_64.AppImage
```

## Project Structure
- `src/frame_work` — framework core for building Qt applications
  - `base/src/` — base components:
    - `platform/` — platform abstraction (ShutdownHandler for Windows/Unix)
    - `process/` — process management (QProcessManager)
    - `include/` — plugin system (QPluginManager, QPluginInterface)
- `src/apps/Daqster` — host application (inherits framework components)
- `src/plugins` — plugins (NodeEditor, QtCoinTrader, examples/tests)
- `src/external_libs` — external dependencies (nodeeditor, qtrest_lib)
- `tools/` — build and packaging tools
  - `create_appimage.sh` — unified script for AppImage creation
  - `Build_AppImage/` — directory for local AppImage builds
- `Docs/` — documentation
  - `Architecture.md` — project architecture
  - `FrameworkAPI.md` — API Reference for framework components
  - `Architecture.md` — project architecture (diagrams in `Docs/diagrams/`)
  - `DeveloperGuide.md` — developer getting-started & contribution guide
  - `HowToDebugAppImage.md` — guide for debugging AppImage
  - `PluginDependencyManagement.md` — plugin dependency system

### Quick Links

- [Framework API Reference](./Docs/FrameworkAPI.md)
- [Architecture Overview](./Docs/Architecture.md)
- [Developer Guide](./Docs/DeveloperGuide.md)
- [Plugin Dependency Management](./Docs/PluginDependencyManagement.md)

## Build Types

### Debug Build
- **Purpose:** Development and debugging
- **Features:** Debug symbols, optimizations disabled, verbose logging
- **Command:** `cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug`

### Release Build
- **Purpose:** Production and performance
- **Features:** Optimizations enabled, debug symbols disabled
- **Command:** `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release`

## AppImage Support

### Local creation
```bash
# Automatic (recommended)
./tools/create_appimage.sh

# With custom parameters
./tools/create_appimage.sh --qt-dir /path/to/qt --source-dir /path/to/build
```

### CI/CD
AppImage is created automatically on every push/PR in GitHub Actions:
- **Release AppImage** - optimized for distribution
- **Debug AppImage** - with debug symbols for debugging

### Using AppImage
```bash
# Run
./Daqster-x86_64.AppImage

# With arguments (to run specific plugin)
./Daqster-x86_64.AppImage QtCoinTrader

# Debug mode
QT_DEBUG_PLUGINS=1 ./Daqster-x86_64.AppImage
```

## Dependency Management

The project supports conditional compilation of components based on available Qt modules:

### Qt5 Support
- **Full support** - all plugins and external libraries (if required Qt modules are available)
- **NodeEditor plugin** - requires `QtMultimedia` + NodeEditor library
- **QtCoinTrader plugin** - requires `QtQuickControls2` + QtRest library
- **Test plugins** - always enabled (basic Qt dependencies)

### Qt6 Support
- **Limited support** - only test plugins (external libraries disabled due to compatibility)
- **Auto-detection** - system automatically selects appropriate components

### Debug Information
CMake shows clear information about which components are enabled/disabled and why:
```
=== External Libraries Status ===
NodeEditor library enabled for Qt5 (Multimedia available)
QtRest library disabled for Qt5 (QuickControls2 not available)

=== Plugins Status ===
NodeEditor plugin enabled for Qt5 (Multimedia + NodeEditor library available)
QtCoinTrader plugin disabled for Qt5 (QuickControls2 not available)
Test plugins enabled for Qt5 (plugin_fancy_test, plugin_main_test, ...)
```

## Notes
- **Build system:** CMake 3.16+
- **Qt versions:** Qt5.15.2+ (full support), Qt6.x (limited support)
- **AppImage:** Universal Linux package, works on all distributions
- **Plugin system:** Dynamic loading with automatic discovery
- **Dependency Management:** Conditional compilation based on available Qt modules


