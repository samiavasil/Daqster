# REQ-SW-PL-016: Auto-Layout for Dependency Graph (Sugiyama)

- **Статус:** DONE
- **Приоритет:** Medium
- **Отговорник (роля):** Ivan (Implementation)
- **Дата:** 2026-08-03
- **Родител:** REQ-SW-PL-009
- **Зависи от:** REQ-SW-PL-009

## Описание

Sugiyama auto-layout за dependency графа на Requirements Manager. Текущият
layout е Kahn-базиран (слоеве в колони по X, left-right по дизайн), но без
минимизация на кръстосвания и без подравняване. Това изискване добавя фаза 2
(crossing minimization с barycenter heuristic) и фаза 3 (координатно
разпределение с size-aware X и вертикално центриране) върху вече готовото
layering (фаза 1 — Kahn + cycle-safe residual слой).

## Acceptance Criteria

- [x] 1. Подравнени слоеве: всички възли в един слой споделят една и съща X координата; X нараства монотонно по слоеве.
- [x] 2. Минимизация на кръстосванията: barycenter sweep (4–6 итерации) намалява кръстосванията; на fixture A→C, A→D, B→C (A,B в ляв слой; C,D в десен) — 1 кръстосване при ID-ред → 0 след Sugiyama.
- [x] 3. Детерминизъм: два `build()`-а на едни и същи данни дават идентични позиции (tie-break: case-insensitive ID + req index).
- [x] 4. Cycle-safe: residual слой за членовете на dependency цикъл, винаги терминира; цикличните ребра стават same-layer и не участват в междуслойните кръстосвания.
- [x] 5. Parent ребрата участват в ordering-а (минимизация на кръстосвания), но НЕ в layering-а — dependency инвариантът `layer[from] < layer[to]` важи.
- [x] 6. Qt5 + Qt6 билдове и тестове: съществуващите тестове остават зелени + новите TestGraphLayout слотове (6) зелени и на двете версии.

## Проследимост

- **Коммити:** *(попълва се при комит)* — branch `feat/phase3-graph-matrix`
- **Код:** `src/plugins/requirements_manager/DependencyGraphLayout.{h,cpp}` (Sugiyama фази 2 & 3), `src/plugins/requirements_manager/DependencyGraphData.{h,cpp}` (фаза 1 + интеграция), `src/plugins/requirements_manager/DependencyGraphWidget.cpp` (чете `GraphNode::width`)
- **Документация:** `docs/Architecture/plugins/README.md` (Requirements Manager + Dependency Graph section)
- **Тестове:** Qt5 + Qt6 builds; `tests/plugins/requirements_manager/test_graph_layout.{h,cpp}` (TestGraphLayout, 6 теста) в shared binary `requirements_manager_tests`; 68/68 PASS на Qt5/Qt6 (requirements_manager suite)

## Бележки по имплементацията

- Фаза 1 (Kahn + residual) в `DependencyGraphData::build()` НЕ е променена —
  `layerFor()` стойностите са идентични с предишния layout.
- Константите `260/110` се местят от `DependencyGraphData.cpp` в
  `DependencyGraphLayout` (координатна политика).
- `GraphNode::width` (нова) носи визуалната ширина на възела
  (`qMax(120.0, 36.0 + title.size()*6.5)`) — същата формула като widget-а;
  layout-ът раздалечава слоевете при дълги заглавия, за да няма overlap.
- Вертикалното центриране е `y_offset = (maxLayerHeight - layerHeight) / 2`
  (неотрицателно) — всеки слой се балансира около средата на най-високия.
- `crossingCount()` е двунивов брояч (O(E log E) с Fenwick tree) между съседни
  слоеве; ребра със споделен край (fan-in/fan-out) не се броят за кръстосване.
