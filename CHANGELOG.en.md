# Changelog
All notable changes to this project will be documented in this file.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and
the project uses [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- **Framework Architecture Refactoring** - major refactoring to extract reusable components:
  - **Platform Abstraction Layer** (`frame_work/base/src/platform/`):
    - `ShutdownHandler` - abstract base class for graceful shutdown
    - `UnixShutdownHandler` - SIGINT/SIGTERM signal handling for Unix/Linux
    - `WindowsShutdownHandler` - Windows console events + stdin fallback
  - **Process Management Layer** (`frame_work/base/src/process/`):
    - `QProcessManager` - generic base class for managing child processes
    - Handle-based process tracking
    - Graceful terminate with force-kill fallback
    - Virtual hooks for customization (setupProcessEnvironment, onAllProcessesFinished)
  - **ApplicationsManager Refactoring**:
    - Inherits from `Daqster::QProcessManager`
    - Backward compatibility preserved (type aliases, event mappings)
    - Daqster-specific environment setup (plugin paths, AppImage detection, XDG directories)
    - Signal forwarding (ProcessEvent → ApplicationEvent)
  - **Cross-platform Support**:
    - Platform-independent shutdown mechanism
    - Proper Ctrl+C handling on Windows and Unix
    - Graceful process termination on all platforms
- **PluginDependencyManager System** - automatic plugin dependency management system
  - `cmake/PluginDependencyManager.cmake` - core dependency management system
  - `cmake/PluginExamples.cmake` - usage examples for the system
  - `Docs/PluginDependencyManagement.md` - comprehensive documentation
- **Automatic Plugin Management**:
  - Automatic detection of Qt modules, external libraries and packages
  - Conditional plugin compilation based on available dependencies
  - Detailed debug information about plugin status
  - Support for Qt5 (full functionality) and Qt6 (limited functionality)
- **External Library Integration**:
  - Qt5: NodeEditor + QtRest libraries included
  - Qt6: External libraries disabled due to compatibility issues
- **Enhanced Build System**:
  - `register_plugin()` function for easy plugin registration
  - Automatic dependency checking
  - Conditional plugin subdirectory inclusion
  - Build configuration and plugin status summaries

## [0.2.0] - 2025-09-18

### Added
- **Unified AppImage build system** - single script `tools/create_appimage.sh` for local and CI AppImage creation
- **GitHub Actions CI/CD** - automated builds and releases with AppImage artifacts
- **Debug AppImage support** - separate Debug and Release AppImage builds
- **Comprehensive documentation**:
  - `Docs/Architecture.md` and `Docs/Architecture.en.md` with PlantUML diagrams
  - `Docs/HowToDebugAppImage.md` - guide for debugging AppImage
  - Updated README files with detailed build and environment variable instructions
- **Enhanced plugin system**:
  - Improved environment variable handling for child processes
  - Debug output for plugin discovery paths
  - AppImage detection and adaptive behavior
- **Professional build types** - Debug (for development) and Release (for production)
- **Environment variables documentation** - complete description of all variables for plugin discovery, Qt environment and debugging

### Changed
- **AppImage creation process** - automated with unified script, supports local and CI modes
- **Plugin launching** - improved for AppImage environment with proper environment setup
- **CI/CD workflows** - simplified and optimized for AppImage creation
- **Documentation structure** - organized in `Docs/` directory with PlantUML diagrams

### Fixed
- **Plugin discovery in AppImage** - fixed environment variables for child processes
- **QML loading issues** - fixed QtCoinTrader plugin for proper QML loading
- **CI permissions** - fixed execute permissions for AppImage scripts
- **Plugin launching from menu** - works correctly in AppImage environment

### Technical Details
- **Build system**: CMake 3.16+, Qt 5.15.2, AppImage packaging
- **Plugin architecture**: Dynamic loading with hash-based deduplication
- **Process isolation**: Plugins launched as separate QProcess
- **Cross-platform**: Linux AppImage distribution ready

## [0.1.0] - 2025-08-29
### Added
- Unified CMake build: top-level `CMakeLists.txt` and subdirectories for apps, frame_work, plugins, external_libs.
- `install()` targets for `Daqster`, `frame_work`, `NodeEditorPlugin`, `QtCoinTraderPlugin`.
- RPATH settings for locating shared libraries at runtime (`$ORIGIN/../lib`).
- **Professional plugin discovery system** with priority-based paths:
  - Build directory (highest priority for debug)
  - Environment variables: `DAQSTER_PLUGIN_DIR` (single directory) and `DAQSTER_PLUGIN_PATH` (multiple directories)
  - User plugins: `~/.local/share/daqster/plugins`
  - System plugins: `/usr/lib/daqster/plugins` and `/usr/local/lib/daqster/plugins`
- **Hash-based plugin deduplication** - prevents loading duplicate plugins from different directories.
- **AppImage-ready config system** - config file is created in writable location (`~/.config/Daqster/daqster.ini`).
- README in Bulgarian and separate `README.en.md` in English with detailed plugin discovery instructions.

### Changed
- Removed all qmake `.pro` files.
- Updated `README.md` to CMake-only instructions.
- **Config file location** - from build directory to writable location for AppImage compatibility.

### Notes
- Initial public version prepared for CI/CD setup.
- **Professional plugin architecture** ready for distribution as AppImage or Flatpak.

[0.1.0]: https://github.com/samiavasil/Daqster/releases/tag/v0.1.0


