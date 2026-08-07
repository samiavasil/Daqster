# REQ-SW-FW-002: Plugin Discovery, Persistence & Registry

- **Статус:** DONE
- **Приоритет:** P1
- **Отговорник (роля):** Implementation
- **Дата:** 2026-08-02
- **Родител:** —
- **Зависи от:** —

## Описание

Ретроспективно изискване за разделените класове на Plugin Manager-а в
`frame_work/base`: `PluginDiscovery` (търсене на плъгини), `PluginPersistence`
(INI персистентност) и `PluginRegistry` (единичен източник на истина за
заредените плъгини и обекти).

## Acceptance Criteria

- [x] 1. `PluginDiscovery::addSearchPath()` dedup-ва по `QDir::absolutePath()` и
       пропуска несъществуващи директории.
- [x] 2. Кандидат-файл се определя по съдържание, не само по разширение:
       `isCandidatePluginFile()` изисква `QLibrary::isLibrary()` и приема файл
       по име (съдържа "plugin"), по съседен `.json` метаданни файл или по
       не-празен `QPluginLoader::metaData()`.
- [x] 3. `PluginDiscovery::computeFileHash()` (MD5) е persistence ключ в INI
       секция `[Plugins]` — `PluginPersistence::loadPlugins()`/`savePluginState()`
       четат/пишат записи, групирани по `PLUGIN_HASH`.
- [x] 4. `PluginRegistry` unregister-ва обекти при destroyed: `registerPlugin()`
       свързва `QObject::destroyed` → премахване от картата, а състоянието на
       плъгин дескрипторите се записва през `setPersistenceCallback()`
       (свързан към `PluginPersistence::savePluginState()` в `QPluginManager`).

## Проследимост

- **Коммити:** `6910f2c` (refactor: split QPluginManager into Discovery, Persistence, Registry), `032e357` (fix #110), `04c57f4` (fix: skip non-existent plugin search paths)
- **Код:** `src/frame_work/base/src/discovery/PluginDiscovery.{h,cpp}`, `src/frame_work/base/src/persistence/PluginPersistence.{h,cpp}`, `src/frame_work/base/src/registry/PluginRegistry.{h,cpp}`
- **Тестове:** Qt5 + Qt6 builds
