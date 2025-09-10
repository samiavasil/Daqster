# Daqster 
[English](./README.en.md) | [Български](./README.md)

Daqster is a Qt5-based framework for creating and loading plugins, with a host application and sample plugins.

Site: https://samiavasil.github.io/Daqster/

## Quick Start (CMake)

### 1) Clone
```
git clone https://github.com/samiavasil/Daqster.git
cd Daqster
git submodule update --init --recursive
```

### 2) Configure & Build
```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

### 3) Run from build
```
cd build/bin
./Daqster
```

## Install & Run
Install (to `/usr/local` by default or a custom prefix):
```
cmake --install build --prefix ./install_dir
```
Run:
```
./install_dir/bin/Daqster
```
- Shared libraries are found automatically via RPATH (`$ORIGIN/../lib`).
- Plugins are searched in the following order (by priority):
  1. **Build directory** - `./plugins` and `../lib/daqster/plugins` (for debug)
  2. **Environment variables** - `DAQSTER_PLUGIN_DIR` (single directory) and `DAQSTER_PLUGIN_PATH` (multiple directories, separated by `:`)
  3. **User plugins** - `~/.local/share/daqster/plugins`
  4. **System plugins** - `/usr/lib/daqster/plugins` and `/usr/local/lib/daqster/plugins`

### Environment Variables for Plugin Discovery
- `DAQSTER_PLUGIN_DIR` - sets a single directory for plugins
- `DAQSTER_PLUGIN_PATH` - sets multiple directories separated by `:` (like PATH)

**Examples:**
```bash
# Single directory
DAQSTER_PLUGIN_DIR=/path/to/plugins ./Daqster

# Multiple directories
DAQSTER_PLUGIN_PATH="/path1:/path2:/path3" ./Daqster

# Both together
DAQSTER_PLUGIN_DIR=/path/to/plugins DAQSTER_PLUGIN_PATH="/path1:/path2" ./Daqster
```

## Project Structure
- `src/frame_work` — core for plugin discovery/loading
- `src/apps/Daqster` — host application
- `src/plugins` — plugins (NodeEditor, QtCoinTrader, examples/tests)
- `src/external_libs` — external deps (nodeeditor, qtrest_lib)

## Notes
- Supported build system: CMake.
- Packaging and extended docs can be added later.


