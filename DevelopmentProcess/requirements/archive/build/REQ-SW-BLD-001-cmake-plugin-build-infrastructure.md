# REQ-SW-BLD-001: CMake Plugin Build Infrastructure

- **Статус:** DONE
- **Приоритет:** P1
- **Отговорник (роля):** Implementation
- **Дата:** 2026-08-02
- **Родител:** —
- **Зависи от:** —

## Описание

Ретроспективно изискване за CMake инфраструктурата на Daqster:
`ComponentTemplates.cmake` (create_application / create_internal_library /
create_plugin), `PluginDependencyManager.cmake` (check_plugin_dependencies,
meta-target) и `FindQtVersion.cmake` (Qt5/Qt6 селекция).

## Acceptance Criteria

- [x] 1. `create_plugin()` генерира плъгин таргет (SHARED библиотека) с
       конвенция за `OUTPUT_NAME` — всеки плъгин използва име, съдържащо
       "plugin" (например `RequirementsManagerPlugin`, `NodeEditorPluginIde`,
       `DemoNodeEditorNodesPlugin`, `AiStudioPluginPlugin`), за да може
       `QPluginManager` да го разпознае като плъгин при филтрация.
- [x] 2. `create_application()` (Qt5/Qt6 shared + AppImage target) и
       `create_internal_library()` покриват app/library изграждането; и трите
       макро-та автоматично връзват component зависимости (вкл.
       `Qt${QT_VERSION_MAJOR}::*`, `frame_work`).
- [x] 3. `PluginDependencyManager.cmake` предоставя `check_plugin_dependencies()`
       (проверка на Qt версия/задължителни компоненти) и meta-target
       `daqster_build_externals` за външни зависимости.
- [x] 4. `FindQtVersion.cmake` избира Qt5 или Qt6 по `USE_QT6` + `CMAKE_PREFIX_PATH`
       (откриване на Qt6 при USE_QT6, Qt5 иначе).

## Проследимост

- **Коммити:** `e0f1395` (refactor #8: extract demo nodes into own plugin with INodeProvider), `1fec530` (feat: plugin dependency manager), `4cede60` (refactor: unify Qt5/Qt6 component macros)
- **Код:** `cmake/ComponentTemplates.cmake`, `cmake/PluginDependencyManager.cmake`, `cmake/FindQtVersion.cmake`
- **Тестове:** Qt5 + Qt6 builds на всички плъгини
