# Changelog
Всички значими промени по този проект ще бъдат документирани в този файл.

Форматът следва [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) и
проектът използва [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- **Framework Architecture Refactoring** - голям рефакторинг за извличане на reusable компоненти:
  - **Platform Abstraction Layer** (`frame_work/base/src/platform/`):
    - `ShutdownHandler` - абстрактен базов клас за graceful shutdown
    - `UnixShutdownHandler` - SIGINT/SIGTERM signal handling за Unix/Linux
    - `WindowsShutdownHandler` - Windows console events + stdin fallback
  - **Process Management Layer** (`frame_work/base/src/process/`):
    - `QProcessManager` - generic базов клас за управление на child процеси
    - Handle-based process tracking
    - Graceful terminate с force-kill fallback
    - Virtual hooks за customization (setupProcessEnvironment, onAllProcessesFinished)
  - **ApplicationsManager Refactoring**:
    - Наследява от `Daqster::QProcessManager`
    - Backward compatibility запазена (type aliases, event mappings)
    - Daqster-specific environment setup (plugin paths, AppImage detection, XDG directories)
    - Signal forwarding (ProcessEvent → ApplicationEvent)
  - **Cross-platform Support**:
    - Платформено-независим shutdown механизъм
    - Правилно Ctrl+C handling на Windows и Unix
    - Graceful процес терминация на всички платформи
- **PluginDependencyManager System** - автоматична система за управление на plugin dependencies
  - `cmake/PluginDependencyManager.cmake` - основна система за dependency management
  - `cmake/PluginExamples.cmake` - примери за използване на системата
  - `Docs/PluginDependencyManagement.md` - подробна документация
- **Automatic Plugin Management**:
  - Автоматично откриване на Qt модули, external библиотеки и packages
  - Условно компилиране на plugins според наличните dependencies
  - Подробна debug информация за plugin статус
  - Поддръжка за Qt5 (пълна функционалност) и Qt6 (ограничена функционалност)
- **External Library Integration**:
  - Qt5: NodeEditor + QtRest библиотеки включени
  - Qt6: External библиотеки изключени заради compatibility проблеми
- **Enhanced Build System**:
  - `register_plugin()` функция за лесно регистриране на plugins
  - Автоматично проверяване на dependencies
  - Условно включване на plugin subdirectories
  - Build configuration и plugin status summaries

## [0.2.0] - 2025-09-18

### Added
- **Unified AppImage build system** - единен скрипт `tools/create_appimage.sh` за локално и CI създаване на AppImage
- **GitHub Actions CI/CD** - автоматизирани билдове и releases с AppImage артефакти
- **Debug AppImage support** - отделни Debug и Release AppImage билдове
- **Comprehensive documentation**:
  - `Docs/Architecture.md` и `Docs/Architecture.en.md` с PlantUML диаграми
  - `Docs/HowToDebugAppImage.md` - ръководство за дебъгване на AppImage
  - Обновени README файлове с подробни инструкции за билдове и environment променливи
- **Enhanced plugin system**:
  - Подобрено environment variable handling за child processes
  - Debug output за plugin discovery paths
  - AppImage detection и адаптивно поведение
- **Professional build types** - Debug (за разработка) и Release (за production)
- **Environment variables documentation** - пълно описание на всички променливи за plugin discovery, Qt environment и debugging

### Changed
- **AppImage creation process** - автоматизиран с unified скрипт, поддържа local и CI режими
- **Plugin launching** - подобрено за AppImage среда с правилно environment setup
- **CI/CD workflows** - опростени и оптимизирани за AppImage създаване
- **Documentation structure** - организирана в `Docs/` директория с PlantUML диаграми

### Fixed
- **Plugin discovery в AppImage** - поправени environment variables за child processes
- **QML loading issues** - поправен QtCoinTrader plugin за правилно QML зареждане
- **CI permissions** - поправени execute permissions за AppImage скриптове
- **Plugin launching от меню** - работи правилно в AppImage среда

### Technical Details
- **Build system**: CMake 3.16+, Qt 5.15.2, AppImage packaging
- **Plugin architecture**: Dynamic loading с hash-based deduplication
- **Process isolation**: Plugins се стартират като отделни QProcess
- **Cross-platform**: Linux AppImage distribution готов

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


