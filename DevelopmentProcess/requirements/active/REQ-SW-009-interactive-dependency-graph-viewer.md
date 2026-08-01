# REQ-SW-009: Interactive Dependency Graph Viewer

- **Статус:** DONE
- **Приоритет:** Medium
- **Отговорник (роля):** Ivan (Implementation)
- **Дата:** 2026-07-31
- **Родител:** REQ-SW-001
- **Зависи от:** REQ-SW-007, REQ-SW-008

## Описание

Интерактивен графичен изглед на зависимостите (`QGraphicsView` / графичен визуализатор), показващ възли (изисквания) и насочени ребра (връзки Родител и Зависи от) с възможност за мащабиране и интеракция.

## Acceptance Criteria

- [x] 1. Нов таб "Dependency Graph" в интерфейса, базиран на `QGraphicsView` / `QGraphicsScene`.
- [x] 2. Визуализиране на изискванията като възли и техните връзки като стрелки.
- [x] 3. Интерактивност: клик върху възел селектира изискването в главното дърво.
- [x] 4. Цветово кодиране според статус (ACTIVE vs DONE) и приоритет.

## Проследимост

- **Коммити:** `cf94e9e` (feat) — branch `feat/phase3-graph-matrix`
- **Код:** `src/plugins/requirements_manager/DependencyGraphData.{h,cpp}` (QtCore-only layout), `src/plugins/requirements_manager/DependencyGraphWidget.{h,cpp}` (GUI), `src/plugins/requirements_manager/RequirementsWidget.{h,cpp}` (QTabWidget shell)
- **Документация:** `docs/Architecture/plugins/README.md` (Requirements Manager + Dependency Graph section)
- **Тестове:** Qt5 + Qt6 builds; `tests/plugins/requirements_manager/test_graph.{h,cpp}` (TestGraph, 6 теста — 34/34 green)
