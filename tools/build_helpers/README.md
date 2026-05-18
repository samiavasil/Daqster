# Daqster Build Helpers

Build scripts and utilities for Daqster on different Qt versions.

## Available Scripts

### validate_builds.sh
Quick validation of both Qt5 and Qt6 builds

**Usage:**
```bash
./tools/build_helpers/validate_builds.sh [OPTIONS]

Options:
  --qt5-prefix PATH    Qt5 installation prefix
  --qt6-prefix PATH    Qt6 installation prefix
  --verbose            Enable verbose output
  -h, --help           Show help
```

**Example:**
```bash
# Quick validation of both Qt versions
./tools/build_helpers/validate_builds.sh
```

Exit codes:
- 0: Both builds successful
- 1: Qt5 failed
- 2: Qt6 failed
- 3: Both failed

### build_qt5.sh
Build Daqster with Qt5.15.2

**Usage:**
```bash
./tools/build_helpers/build_qt5.sh [OPTIONS]

Options:
  --clean              Clean build directory before building
  --qt-prefix PATH     Qt5 installation prefix (default: $HOME/bin/Qt/5.15.2/gcc_64)
  --build-dir DIR      Build directory name (default: build-qt5-checkpoint)
  --verbose            Enable verbose output
  -h, --help           Show help
```

**Examples:**
```bash
# Clean rebuild with verbose output
./tools/build_helpers/build_qt5.sh --clean --verbose

# Build with custom prefix
./tools/build_helpers/build_qt5.sh --qt-prefix /opt/Qt/5.15.2
```

### build_qt6.sh
Build Daqster with Qt6.6.3

**Usage:**
```bash
./tools/build_helpers/build_qt6.sh [OPTIONS]

Options:
  --clean              Clean build directory before building
  --qt-prefix PATH     Qt6 installation prefix (default: $HOME/bin/Qt/6.6.3/gcc_64)
  --build-dir DIR      Build directory name (default: build-qt6-checkpoint)
  --verbose            Enable verbose output
  -h, --help           Show help
```

**Examples:**
```bash
# Build with Qt6
./tools/build_helpers/build_qt6.sh --clean

# Custom build directory
./tools/build_helpers/build_qt6.sh --build-dir my_build_qt6
```

### run_cointrader_wsl.sh
Launch QtCoinTrader plugin with WSL-optimized environment

**Usage:**
Copy this script to your build directory and run:
```bash
./run_cointrader_wsl.sh
```

Or use directly with absolute path:
```bash
./tools/build_helpers/run_cointrader_wsl.sh
```

**What it does:**
- Sets `QT_XCB_FORCE_SOFTWARE_OPENGL=1` for stable rendering on WSL
- Configures OpenSSL 1.1 runtime library path for SSL compatibility
- Avoids GPU-related framebuffer issues common on virtualized environments

## Quick Start

### 1. Build Qt5 version
```bash
cd ~/Projects/Qt/Daqster
./tools/build_helpers/build_qt5.sh --clean --verbose
```

### 2. Run QtCoinTrader
```bash
cd build-qt5-checkpoint/bin
LD_LIBRARY_PATH="$HOME/bin/openssl11/pkg/usr/lib/x86_64-linux-gnu:${LD_LIBRARY_PATH}" \
  QT_XCB_FORCE_SOFTWARE_OPENGL=1 ./Daqster QtCoinTrader
```

Or use the helper script:
```bash
cp tools/build_helpers/run_cointrader_wsl.sh build-qt5-checkpoint/bin/
cd build-qt5-checkpoint/bin
./run_cointrader_wsl.sh
```

### 3. Build Qt6 version
```bash
./tools/build_helpers/build_qt6.sh --clean
```

Note: Qt6 build has limited plugin support due to missing modules (Charts, Multimedia, WebSockets).

## Environment Variables

### Important for WSL
- `QT_XCB_FORCE_SOFTWARE_OPENGL=1` - Use software rendering instead of GPU (avoids FBO issues)
- `LD_LIBRARY_PATH` - Point to OpenSSL 1.1 for SSL compatibility with Qt5 runtime

### Optional Performance Tuning
- `QT_QPA_GL_GLES2=1` - Use ANGLE backend on WSL2 (if available)
- `DAQSTER_VERBOSE_DEPENDENCIES=ON` - Enable verbose CMake output

## Troubleshooting

### Black/invisible UI widgets
- Ensure QML import paths are set correctly (check QtCoinTraderPluginObject.cpp)
- Verify all QML files are present in build/plugins directory

### OpenGL framebuffer errors
- This is expected on WSL with virtualized GPU
- Use `QT_XCB_FORCE_SOFTWARE_OPENGL=1` to bypass GPU

### SSL/certificate errors  
- WSL may have certificate issues
- Local OpenSSL 1.1 is available at `$HOME/bin/openssl11/`
- Runtime includes workaround in launcher scripts

### Build fails for Qt6
- Check if required modules are installed: `qtcharts`, `qtmultimedia`, `qtwebsockets`
- Install with: `aqt install-qt linux desktop 6.6.3 gcc_64 -O ~/bin/Qt -m qtcharts qtmultimedia qtwebsockets`

## Legacy Scripts (Deprecated)

**Removed scripts:**
- `test_qt_versions.sh` - Replaced by `validate_builds.sh`
- `test_qt6_qtrest.sh` - Replaced by `build_qt6.sh` and `validate_builds.sh`

These test scripts were used for validating Qt version detection but are superseded by the new build system.
If you need to check specific Qt version compatibility, use `validate_builds.sh` instead.
