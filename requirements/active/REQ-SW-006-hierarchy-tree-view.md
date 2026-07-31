# REQ-SW-006: Hierarchy Tree View

- **Статус:** ACTIVE
- **Приоритет:** High
- **Отговорник (роля):** Implementation
- **Дата:** 2026-07-31
- **Родител:** REQ-SW-001
- **Зависи от:** REQ-SW-002

## Описание

Реализиране на йерархичен изглед (`QTreeView`) в лявото поле на инструмента, групиращ изискванията по техния `Родител:` (parent-child relationship) вместо плосък списък.

## Acceptance Criteria

- [ ] 1. `RequirementsTreeModel` наследява `QAbstractItemModel` или `QStandardItemModel` за организиране на дървовидна структура според полето `Родител:`.
- [ ] 2. `QTreeView` визуализира йерархията с възможност за разтваряне/свиване на възли.
- [ ] 3. Синхронизация между избора в дървото и детайлния панел вдясно.

## Проследимост

- **Коммити:** —
- **Код:** `src/plugins/requirements_manager/`
- **Документация:** `docs/Architecture/plugins/`
- **Тестове:** Qt5 + Qt6 builds
