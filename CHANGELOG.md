# Changelog
Всички значими промени по този проект ще бъдат документирани в този файл.

Форматът следва [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) и
проектът използва [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- **Video нодове** (`src/plugins/demo_nodeditor_nodes/Sources/Video/`):
  - `VideoCompat.h` — Qt5/Qt6 multimedia абстракция (QVideoProbe ↔ QVideoSink, camera enumeration, media source assignment, playback-state сигнали)
  - `CameraSourceNode` — заснемане от QCamera (default или избран device)
  - `VideoFileSourceNode` — възпроизвеждане на локален видео файл (QMediaPlayer)
  - `StreamSourceNode` — възпроизвеждане на HTTP/RTSP stream (QMediaPlayer)
  - `VideoOutputNode` — live preview на incoming кадри (QLabel, pass-through)
  - `VideoModifierNode` → `VideoTransformNode` (REQ-SW-PL-019) — конфигурируем transform node: 8 базови операции (RGB Channel Swap, Grayscale, Invert, Brightness, Contrast, Blur, Flip, Sepia) + опционални OpenCV операции (GaussianBlur, Canny, Threshold) при открит OpenCV
  - Регистрирани под категория "Video" в demo node editor plugin
- **Requirements Manager plugin** (`src/plugins/requirements_manager/`):
  - Standalone application plugin с requirements дърво (REQ-SW-002..008)
  - Навигация: auto-clear filter при линк клик + back/forward история
  - Search engine (REQ-SW-PL-011)
  - Multi-repository requirements view — merge на public/private requirements (REQ-SW-PL-012)
- **Dependency graph viewer** (REQ-SW-009) — интерактивен граф на зависимостите между requirements
- **Traceability matrix** (REQ-SW-010) — изглед и export на traceability матрицата
- **Typed requirement IDs** — поддръжка на `REQ-SW-<TYPE>-NN` схема с формат валидация (с миграция на съществуващите ID-та)
- **Sugiyama auto-layout** (REQ-SW-PL-016) — автоматично подреждане на dependency graph
- **Phase status indicators** (REQ-SW-PL-017) — индикатори за фаза/статус в requirements main view
- **Plugin security & vendor trust store** (REQ-SW-FW-007) — ново изискване за plugin сигурност
- **ArithmeticLogic нод** (`BuiltInNodes/Operators/ArithmeticLogic/`):
  - `ExprParser` — recursive descent C++ expression evaluator (всички C/C++ оператори: `+`,`-`,`*`,`/`,`%`,`&`,`|`,`^`,`~`,`<<`,`>>`,`&&`,`||`,`!`,`==`,`!=`,`<`,`>`,`<=`,`>=`,`?:`)
  - `ArithmeticLogicModel` — конфигурируем нод: тип (int/double), 2–8 входа, expression field, optional strobe
  - Променливи `a`–`h` отговарят на входните портове
- **NodeEditorWidget Shared Component** (`src/plugins/node_editor_widget/`):
  - `NodeEditorWidget` - споделен Qt Widgets GUI за node-based редактори
  - `ChatGraphModel` - loop-enabled графичен модел
  - `node_editor_widget_global.h` - export macro (`NODE_EDITOR_WIDGET_EXPORT`)
  - Автоматична инжекция на стандартните Daqster ноди (Audio, Media, Graphs, AI, etc.)
- **NodeEditorApp Plugin** (`src/plugins/node_editor_app/`):
  - Базов графичен плъгин използващ `NodeEditorWidget`
  - `APPLICATION_PLUGIN` тип с `create_plugin()` макро
- **ChatGraphModel в споделената библиотека** — преместен от `node_editor_ide/` в `BuiltInNodes/Library/types/` за generality

### Changed
- **Directory Restructuring**:
  - `src/external_libs/` → `src/plugins/external_libs/` (всички external libs са под plugins)
  - `src/plugins/node_editor/` → разделяне на `node_editor_widget/` + `node_editor_app/`
  - `.gitmodules` paths актуализирани
- **nodeeditor target**: Променен от `nodes` на `QtNodes` (pin commit `4709573`)
- **cmake/ComponentTemplates.cmake**: `create_external_library()` path → `src/plugins/external_libs/`
- **CI Workflow**: Добавени Python patch стъпки за qtrest install fix (cmake_install.cmake patching)
- **Architecture docs** актуализирани за новата структура

### Fixed
- **Plugin launch fixes**:
  - Toolbar пуска plugins по име вместо stale hash
  - Prune на persisted plugin entries с несъответстващ файлов hash при load
  - Явна грешка (qCCritical + QMessageBox) при неуспешно зареждане на plugin
  - AppSelectionDialog съхранява visibility по plugin name
- **Video нодове** — компилация на Qt5 и Qt6 (VideoCompat helpers: connectPlayerError/connectCameraError, variantToInt за Qt5 QVariant, QOverload за error signal)
- **Dependency graph** — edges следват nodes при drag, viewport се fit-ва при resize
- **Requirements Manager** — dedup на requirement файлове, достижими през множество roots, предупреждение при добавяне на overlapping root
- **NumericType::numberAsText()** — fix за ambiguous overload при int тип: явно cast до `double` с precision 0, предотвратява показване на hex/placeholder стойности вместо числа

### Removed
- `src/plugins/node_editor/` — монолитен plugin (заменен от widget + app)
- `src/external_libs/` — празна директория премахната

- **Framework Architecture Refactoring** - голям рефакторинг за извличане на reusable компоненти:
  - **Platform Abstraction Layer** (`frame_work/base/src/platform/`):
    - `ShutdownHandler` - абстрактен базов клас за graceful shutdown
    - `UnixShutdownHandler` - SIGINT/SIGTERM signal handling за Unix/Linux (self-pipe)
    - `WindowsShutdownHandler` - Windows console events (SetConsoleCtrlHandler)
    - `QConsoleListener` - stdin-базиран quit/exit handler (крос-платформен, в Daqster приложението)
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
  - `docs/PluginDependencyManagement.md` - подробна документация
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
  - `docs/Architecture.md` и `docs/Architecture.en.md` с PlantUML диаграми
  - `docs/HowToDebugAppImage.md` - ръководство за дебъгване на AppImage
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
- **Documentation structure** - организирана в `docs/` директория с PlantUML диаграми

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


