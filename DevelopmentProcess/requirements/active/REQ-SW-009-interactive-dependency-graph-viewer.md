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

- **Коммити:** `cf94e9e` (feat) — branch `feat/phase3-graph-matrix`; фикс на плъзгането на възли + фит на изгледа (2026-08-02): `17ed818` (fix)
- **Код:** `src/plugins/requirements_manager/DependencyGraphData.{h,cpp}` (QtCore-only layout), `src/plugins/requirements_manager/DependencyGraphWidget.{h,cpp}` (GUI), `src/plugins/requirements_manager/RequirementsWidget.{h,cpp}` (QTabWidget shell)
- **Документация:** `docs/Architecture/plugins/README.md` (Requirements Manager + Dependency Graph section)
- **Тестове:** Qt5 + Qt6 builds; `tests/plugins/requirements_manager/test_graph.{h,cpp}` (TestGraph, 6 теста) + `tests/plugins/requirements_manager/test_graph_widget.{h,cpp}` (GUI binary `requirements_manager_gui_tests`, 2 регресионни слота: `edgeFollowsNodeMove`, `fitsViewportAfterResize`; 4/4 green с init/cleanup); smoke harness с реална drag симулация — 35/35 + 4/4 green (Qt5 + Qt6)

### Добавки 2026-08-02 (плъзгане на възли + фит на изгледа)

- **Коренна причина (Bug A):** ребрата бяха статични пътища, изчислени веднъж в `setRequirements` с твърдо `fromRadius = 60.0`; `DependencyGraphNodeItem` имаше `ItemIsMovable | ItemSendsGeometryChanges`, но **без** override на `itemChange()` — флагът беше мъртъв. Освен това `DependencyGraphView` работеше в режим `ScrollHandDrag`, който поглъщаше плъзгането на мишката върху възлите — реално влачене не местеше възела изобщо.
- **Решение:** нов `DependencyGraphEdgeItem` (държи `from`/`to`, `updateGeometry()`), `makeEdgePath(fromRect, toRect, ...)` с реални половин-размери на всеки възел (`halfSizeAlongDirection`), `itemChange()` → `positionChanged` сигнал, свързан към `updateGeometry()`; `DependencyGraphView::mousePressEvent` превключва към `NoDrag` само за натискания върху движим елемент (празното място запазва scroll-hand панорамирането).
- **Коренна причина (Bug B):** графиката се побираше веднъж в малкия пре-resize viewport (scale 0.033) и никога не се пре-фитваше след resize/show.
- **Решение:** `setAutoFit(true)` в `setRequirements`, `resizeEvent`/`showEvent` → `maybeFitToScene()`, `wheelEvent` изключва auto-fit (потребителският zoom не се затрива); `setSceneRect(itemsBoundingRect().adjusted(-60,-60,60,60))` се преизчислява след движение на възел.
