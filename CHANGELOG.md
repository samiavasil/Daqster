# Changelog
Всички значими промени по този проект ще бъдат документирани в този файл.

Форматът следва [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) и
проектът използва [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.1.0] - 2025-08-29
### Added
- Единен CMake билд: top-level `CMakeLists.txt` и поддиректории за apps, frame_work, plugins, external_libs.
- `install()` цели за `Daqster`, `frame_work`, `NodeEditorPlugin`, `QtCoinTraderPlugin`.
- RPATH настройки за runtime намиране на библиотеки (`$ORIGIN/../lib`).
- **Professional plugin discovery system** с приоритетни пътища:
  - Build директория (най-висок приоритет за дебъг)
  - Environment variables: `DAQSTER_PLUGIN_DIR` (една директория) и `DAQSTER_PLUGIN_PATH` (множество директории)
  - User plugins: `~/.local/share/daqster/plugins`
  - System plugins: `/usr/lib/daqster/plugins` и `/usr/local/lib/daqster/plugins`
- **Hash-based plugin deduplication** - предотвратява дублиране на същите plugins в различни директории.
- **AppImage-ready config system** - config файл се създава в writable location (`~/.config/Daqster/daqster.ini`).
- README на български и отделно `README.en.md` на английски с подробни инструкции за plugin discovery.

### Changed
- Премахнати всички qmake `.pro` файлове.
- Обновен `README.md` към CMake-only инструкции.
- **Config файл location** - от build директория към writable location за AppImage compatibility.

### Notes
- Начална публична версия, подготвена за CI/CD настройка.
- **Professional plugin architecture** готова за distribution като AppImage или Flatpak.

[0.1.0]: https://github.com/samiavasil/Daqster/releases/tag/v0.1.0


