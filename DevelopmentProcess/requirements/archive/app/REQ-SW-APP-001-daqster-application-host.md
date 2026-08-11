# REQ-SW-APP-001: Daqster Application Host

- **Статус:** DONE
- **Приоритет:** P1
- **Отговорник (роля):** Implementation
- **Дата:** 2026-08-02
- **Родител:** —
- **Зависи от:** REQ-SW-FW-001, REQ-SW-FW-004, REQ-SW-FW-005

## Описание

Ретроспективно изискване за хост приложението `src/apps/Daqster` (`main.cpp` +
`ApplicationsManager` + `AppToolbar` + `AppSelectionDialog` + `QConsoleListener`):
инициализация на плъгините, стартиране на application плъгини като дъщерни
процеси (CLI/headless), toolbar с меню и kill контроли, настройки за тема.

## Acceptance Criteria

- [x] 1. `PluginsInit()`: `SearchForPlugins()` → `GetPluginList()` →
       `CreatePluginObject()` за enabled плъгини → `deleteLater()` при шутдаун.
- [x] 2. Headless/CLI режим през `ApplicationsManager` (наследник на
       `QProcessManager`): `StartApplication()` делегира на `StartProcess()`,
       `SetHeadlessMode()` + `onAllProcessesFinished()` → `qApp->quit()`.
- [x] 3. `AppToolbar` е movable/floatable `QToolBar` с Menu (Setup / Debug Console
       / Quit) и kill контроли (Stop All + per-process dropdown от
       `ApplicationsManager::ApplicationEvent`).
- [x] 4. `AppSelectionDialog` съхранява тема (System / Dark) в QSettings "Daqster"
       (`Theme/Style`) и прилага stylesheet от `:/toolbar/icons/StyleFile` при dark.

## Проследимост

- **Коммити:** `0907d06` (feat: add ShutdownHandler factory + cross-platform paths), `9ea787c` (fix: correct AppImage plugin/library paths in ApplicationsManager), `6b23593` (fix: CLI name fallback and disabled plugin loading)
- **Код:** `src/apps/Daqster/main.cpp`, `ApplicationsManager.{h,cpp}`, `AppToolbar.{h,cpp}`, `AppSelectionDialog.{h,cpp}`, `QConsoleListener.{h,cpp}`
- **Тестове:** Qt5 + Qt6 builds; headless smoke (offscreen)
