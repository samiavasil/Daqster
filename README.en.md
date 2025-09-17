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
- `src/frame_work` — core for plugin discovery/loading
- `src/apps/Daqster` — host application
- `src/plugins` — plugins (NodeEditor, QtCoinTrader, examples/tests)
- `src/external_libs` — external dependencies (nodeeditor, qtrest_lib)
- `tools/` — build and packaging tools
  - `create_appimage.sh` — unified script for creating AppImage
  - `Build_AppImage/` — directory for local AppImage builds
- `Docs/` — documentation
  - `Architecture.md` — project architecture
  - `HowToDebugAppImage.md` — guide for debugging AppImage

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

## Notes
- **Build system:** CMake 3.16+
- **Qt version:** 5.15.2 (local), 5.15.2+ (CI)
- **AppImage:** Universal Linux package, works on all distributions
- **Plugin system:** Dynamic loading with automatic discovery


