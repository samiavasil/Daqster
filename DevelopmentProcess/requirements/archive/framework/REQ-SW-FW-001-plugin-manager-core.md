# REQ-SW-FW-001: Plugin Manager Core

- **Статус:** DONE
- **Приоритет:** P1
- **Отговорник (роля):** Implementation
- **Дата:** 2026-08-02
- **Родител:** —
- **Зависи от:** REQ-SW-FW-002

## Описание

Ретроспективно изискване за ядрото на Plugin Manager-а (`QPluginManager` +
`QBasePluginObject` + `QPluginLoaderExt`) в `frame_work/base`: създаване на
плъгин обекти, включване/изключване с персистентност, коректно изключване
(shutdown) с state machine и разделни Qt5/Qt6 конфигурационни файлове.

## Acceptance Criteria

- [x] 1. `CreatePluginObject()` създава плъгин обект: зареденият `QPluginLoaderExt`
       (`LoadPluginInterfaceObject`) регистрира `QPluginInterface` в
       `PluginRegistry` (`registerPlugin`), а самият обект се създава през
       `PluginRegistry::createPluginObject()` → `QPluginInterface::CreatePlugin()`.
- [x] 2. `EnableDisablePlugin()` синхронизира enabled състоянието с persistency
       callback: промяната минава през `PluginDescription::Enable()` +
       `m_registry->setPluginDescription()` + `object->Enable()` и се записва в
       INI конфиг чрез `m_persistence->savePluginState()`; при изключване се
       вика `ShutdownPlugin()`.
- [x] 3. Шутдаун: `ShutdownPluginManager()` (свързан към `QApplication::aboutToQuit`,
       защитен с `static bool s_shutdownDone`) вика `m_registry->shutdownAll()`
       → `ShutdownAllPluginObjects()` → `ShutdownPluginObject()` (state machine
       WORKING → SHUTTING_DOWN → TURNED_OFF + `deleteLater`); `QPluginLoaderExt`
       skip-ва unload на библиотеката при шутдаун (`setShuttingDown(true)`).
- [x] 4. Qt5/Qt6 конфиг файлове се използват разделно
       (`daqster_qt5.ini` / `daqster_qt6.ini` под `AppConfigLocation`) — AppImage
       съвместимост, без взаимна корупция при едновременно стартиране.

## Проследимост

- **Коммити:** `032e357` (fix #110: dedup + stale pruning), `5731808` (refactor: merge CreatePluginObject into PluginRegistry), `be51f46` (fix: separate Qt5/Qt6 config files), `61d93f5` (fix: prevent double ShutdownPluginManager)
- **Код:** `src/frame_work/base/src/QPluginManager.cpp`, `QBasePluginObject.cpp`, `QPluginLoaderExt.{h,cpp}`, `QPluginInterface.cpp`, `registry/PluginRegistry.cpp`
- **Тестове:** Qt5 + Qt6 builds

### Бележка

Hardcoded plugin search paths (`/usr/lib/daqster/plugins`,
`/usr/local/lib/daqster/plugins`, `C:/Program Files/Daqster/plugins` и
`QDir::homePath() + "/.local/share/daqster/plugins"`) са известен риск за
Windows (пътят не е под `%APPDATA%`, а само `GenericDataLocation/Daqster/plugins`),
и за системни инсталации — те са документирани като известен риск, а не като
завършена абстракция на платформените локации.
