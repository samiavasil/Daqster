# Changelog
All notable changes to this project will be documented in this file.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and
the project uses [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

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


