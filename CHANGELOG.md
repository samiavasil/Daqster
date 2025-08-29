# Changelog
Всички значими промени по този проект ще бъдат документирани в този файл.

Форматът следва [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) и
проектът използва [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.1.0] - 2025-08-29
### Added
- Единен CMake билд: top-level `CMakeLists.txt` и поддиректории за apps, frame_work, plugins, external_libs.
- `install()` цели за `Daqster`, `frame_work`, `NodeEditorPlugin`, `QtCoinTraderPlugin`.
- RPATH настройки за runtime намиране на библиотеки (`$ORIGIN/../lib`).
- Разширено търсене на плъгини: `./plugins`, `../lib/daqster/plugins`, `DAQSTER_PLUGIN_DIR`.
- README на български и отделно `README.en.md` на английски.

### Changed
- Премахнати всички qmake `.pro` файлове.
- Обновен `README.md` към CMake-only инструкции.

### Notes
- Начална публична версия, подготвена за CI/CD настройка.

[0.1.0]: https://github.com/samiavasil/Daqster/releases/tag/v0.1.0


