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
- Plugins are searched in:
  - `./plugins` (next to the binary),
  - `../lib/daqster/plugins` (install path),
  - a directory set via environment variable `DAQSTER_PLUGIN_DIR`.

## Project Structure
- `src/frame_work` — core for plugin discovery/loading
- `src/apps/Daqster` — host application
- `src/plugins` — plugins (NodeEditor, QtCoinTrader, examples/tests)
- `src/external_libs` — external deps (nodeeditor, qtrest_lib)

## Notes
- Supported build system: CMake.
- Packaging and extended docs can be added later.
