# Changelog
All notable changes to this project will be documented in this file.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and
the project uses [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.1.0] - 2025-08-29
### Added
- Unified CMake build: top-level `CMakeLists.txt` and subdirectories for apps, frame_work, plugins, external_libs.
- `install()` targets for `Daqster`, `frame_work`, `NodeEditorPlugin`, `QtCoinTraderPlugin`.
- RPATH settings for locating shared libraries at runtime (`$ORIGIN/../lib`).
- Extended plugin search paths: `./plugins`, `../lib/daqster/plugins`, `DAQSTER_PLUGIN_DIR` environment variable.
- README in Bulgarian and separate `README.en.md` in English.

### Changed
- Removed all qmake `.pro` files.
- Updated `README.md` to CMake-only instructions.

### Notes
- Initial public version prepared for CI/CD setup.

[0.1.0]: https://github.com/samiavasil/Daqster/releases/tag/v0.1.0
