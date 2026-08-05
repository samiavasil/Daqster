# Plugins

Родител: [Architecture Overview](../README.md) | [Documentation Index](../../INDEX.md)

Съседни подсистеми: [Applications](../apps/README.md) | [Framework](../framework/README.md)

Плъгините добавят функционалности към Daqster приложението. Зареждат се динамично и комуникират чрез дефинирани интерфейси.

## Available Plugins

| Plugin | Локация | Тип | Описание |
|--------|---------|-----|----------|
| **Node Editor IDE** | `src/plugins/node_editor_ide/` | APPLICATION_PLUGIN | Визуален node-based редактор с вградени Audio/LLaMA нодове |
| **Demo NodeEditor Nodes** | `src/plugins/demo_nodeditor_nodes/` | Node Provider | INodeProvider — доставя AudioSource, LLaMA, AudioDisplay, GenericDisplay, Demux/Mux нодове |
| **QtCoinTrader** | `src/plugins/QtCoinTrader/` | APPLICATION_PLUGIN | Cryptocurrency trading plugin |
| **Requirements Manager** | `src/plugins/requirements_manager/` | APPLICATION_PLUGIN | Markdown-based traceable requirements viewer/editor (REQ-SW-PL-*) |

### Plugin документация

Всяка основна плъгин компонента има своя документация:

- [Node Editor IDE](./node_editor_ide/README.md) — архитектура, вградени нодове, INodeProvider discovery
- [Demo NodeEditor Nodes](./demo_nodeditor_nodes/README.md) — INodeProvider пример, доставени нодове

### Requirements Manager (REQ-SW-PL-009 — Interactive Dependency Graph Viewer)

Плъгинът `requirements_manager` (`src/plugins/requirements_manager/`) е Markdown-based
requirements viewer/editor. Изгледите са организирани в `QTabWidget`:

- **Requirements** — дърво + детайли (предишен Phase 1&2 UI).
- **Dependency Graph** — интерактивна визуализация на зависимостите:

  - `DependencyGraphData` (QtCore-only) — граф + **Sugiyama слоест layout**:
    един възел на изискване, ребра `Родител:` и `Зависи от:` (case-insensitive
    resolution), Kahn's algorithm върху dependency ребрата (фаза 1) с
    cycle-safe residual слой и `seen-set` guard (винаги терминира).
    Нерesolv-нати референции се записват като `danglingIds()` и НЕ стават
    ребра. Позициите се изчисляват от `DependencyGraphLayout` (фази 2 & 3).
  - `DependencyGraphLayout` (QtCore-only) — **Sugiyama фази 2 & 3** върху
    layering-а от `DependencyGraphData`:
    - **Фаза 2 — минимизация на кръстосванията (barycenter):**
      `orderLayers()` започва от ID-сортиран ред във всеки слой и прави
      4–6 sweep-а ляво→дясно / дясно→ляво, пренареждайки всеки слой по
      средната позиция на съседите в съседния слой; запазва се редът с
      най-малко кръстосвания (`crossingCount()` — двунивов брояч с инверсии).
      В ordering-а участват **всички** ребра (Parent + Dependency); цикличните
      ребра живеят в residual слоя (same-layer) и не дават междуслойни
      кръстосвания.
    - **Фаза 3 — координати:** `assignCoordinates()` разпределя слоевете по X
      (`x = layer * 260`, разширено при широки заглавия — размерът на възела
      се носи от `GraphNode::width`) и редовете по Y
      (`y = row * 110`) с вертикално центриране на всеки слой спрямо
      най-високия (offset = `(maxLayerHeight - layerHeight) / 2` — винаги
      неотрицателен).
    - Детерминизъм: tie-break-ът е case-insensitive ID + req index; два
      `build()`-а на едни и същи данни дават идентични позиции.
  - `DependencyGraphWidget`/`DependencyGraphScene` (QtWidgets) — `QGraphicsScene`
    с movable rounded-rect възли (ID + title), `QGraphicsPathItem` ребра със
    стрелки (dashed = Parent, solid = Dependency), wheel zoom върху
    `QGraphicsView`, drag (`ItemIsMovable`) и `navigateRequested(id)` при клик
    върху възел — връща потребителя в tree tab и селектира изискването.
  - Цветово кодиране (hex-only, Qt5/Qt6-safe): border = статус
    (ACTIVE `#2E7D32`, DONE `#757575`, CANCELLED `#C62828`), fill = приоритет
    (High по-тъмен / Medium среден / Low по-светъл). Легенда и warning label
    при dangling references.

  - **Ребрата следват възлите при влачене (2026-08-02):** ребрата са
    `DependencyGraphEdgeItem` (държат `from`/`to`), преизчислявани от
    `updateGeometry()` по текущите `scenePos()` + `boundingRect().center()` на
    възлите с реални половин-размери (`halfSizeAlongDirection`), вместо
    статичните пътища с твърд `fromRadius = 60.0`. `DependencyGraphNodeItem`
    вече има `itemChange()` override → `positionChanged` сигнал, свързан към
    `updateGeometry()`. `DependencyGraphView::mousePressEvent` превключва към
    `NoDrag` само при натискане върху движим елемент — плъзгането на възел
    работи с мишката, а празното място запазва scroll-hand панорамирането.
    Изгледът се пре-фитва върху сцената при `resizeEvent`/`showEvent`
    (auto-fit, изключван от wheel zoom), вместо да остава с размер от
    пре-resize viewport-а; `setSceneRect` се преизчислява след движение.

  Тестове: `tests/plugins/requirements_manager/test_graph.{h,cpp}` — TestGraph
  (6 теста) + `tests/plugins/requirements_manager/test_graph_layout.{h,cpp}` —
  **TestGraphLayout (6 теста, Sugiyama фази 2 & 3**: `layersPreserved`,
  `crossingsReduced`, `deterministic`, `alignedAndCentered`,
  `cycleResidualLayerStable`, `parentEdgesDoNotBreakLayering`) в shared binary
  `requirements_manager_tests` (49/49 PASS на Qt5/Qt6);
  `tests/plugins/requirements_manager/test_graph_widget.{h,cpp}` — GUI binary
  `requirements_manager_gui_tests` (2 регресионни слота: `edgeFollowsNodeMove`,
  `fitsViewportAfterResize`; 4/4 green с init/cleanup, offscreen) —
  общо 68/68 PASS на Qt5/Qt6 (requirements_manager suite).

#### Multi-Repository Requirements View (REQ-SW-PL-012 — merged view)

От 2026-08-04 плъгинът може да показва requirements от НЯКОЛКО repo-та наведнъж:

- **Discovery:** `RequirementsParser::discoverRepoRoots()` — върви нагоре от
  директорията на бинарния файл до първия repo root (съдържащ
  `DevelopmentProcess/requirements`), после сканира ПАРЕНТ директорията за
  съседни директории със същата структура. Връща canonical absolute paths,
  дедуплицирани и сортирани. Никъде в кода НЕ са захардкодени пътища.
- **Merge:** `RequirementsParser::parseDirectories(QVector<RequirementRoot>)` —
  всеки root се парсва с `parseDirectory()`, всяко изискване получава
  `Requirement::repo` (от ID префикса: `REQ-SW-*` → `public`, друг `REQ-*` →
  `private`, останалите → `other`), резултатът се stable-sort-ва по
  (id, repo). Бутонът става "Add requirements folder…" (APPEND root, не
  replace); статус линия "N roots loaded".
- **Анотации:** референции като `REQ-SW-PL-013 (публично)` / `(частно)` се
  нормализират до bare ID за resolution, но оригиналният текст се пази в
  `Requirement::dependencyHints` — валидаторът предупреждава (Warning) при
  несъответствие между анотацията и реалния repo на target-а.
- **Shared repo filter:** един `QComboBox` ("All repos" + "public" + "private"
  + по един entry на root при различен label). Widget-ът пази ПЪЛНИЯ набор за
  валидация/preview/actions; ФИЛТРИРАНИЯТ поднабор захранва tree/model, graph
  и matrix. Матрицата НЯМА собствен repo combo.
- **Repo column:** в дървото и traceability matrix-а; "Repo:" линия в preview.
- **Graph:** `GraphNode.repo`; resolution-ът в `DependencyGraphData::build()` е
  repo-aware (same-repo match печели, fallback към първия match) — cross-tree
  ребрата се резолват детерминистично. Дублиран ID в два repo-та е валидатор
  **Error**.
- **Верификация:** Qt5/Qt6 builds + offscreen smoke — приложение с
  RequirementsManager показва 35 merged requirements от двата repo-та
  (`DaqsterAiStudio` + `daqster`). Unit тестове за новите пътища са
  **отложени по решение на потребителя** (съществуващите тестове продължават
  да се компилират; AC не са mark-нати като verified).

#### Requirements Search Engine (REQ-SW-PL-011)

От 2026-08-04 Requirements Manager има full-text search над изискванията:

- **Клас:** `RequirementsSearchEngine.{h,cpp}` (QtCore-only, без `Q_OBJECT` —
  същия стил като `RequirementsValidator`, директно unit-testable).
  `filter()` приема структурираните `Requirement` записи и връща
  case-insensitive multi-term (AND) подмножество; `normalizedText()` изгражда
  lowercased searchable blob (id, title, description, acceptance criteria,
  traceability, commits, code, tests, parentId, dependencies, status, priority,
  assignee, repo, section, fileName, date) — **НЕ индексира** `rawContent` и
  `dependencyHints` (архитектурното правило: search консумира структурирания
  data source, никога не парсва markdown наново).
- **Field prefixes:** `id:`, `status:`, `priority:`, `assignee:`, `repo:`,
  `section:` — split на ПЪРВИЯ colon; непознат key (напр. `http://`) се третира
  като full-text term.
- **UI:** `QLineEdit` с clear button + debounce `QTimer` (~150 ms) в
  `RequirementsWidget`; `applyViewFilters()` композира repo филтъра
  (`filterRequirementsByRepo` / root-path prefix) със search-а
  (`RequirementsSearchEngine::filter`). И трите изгледа — tree/model, graph и
  matrix — консумират ЕДНО и също филтрирано подмножество; FULL set остава за
  validation/preview/edit/save/archive (еднаква архитектура като PL-012).
- **Relations search:** тъй като `parentId` и `dependencies` са част от
  normalized blob-а, въвеждането на родител/dependency ID намира свързаните
  изисквания (AC3).
- **Верификация:** Qt5/Qt6 builds + offscreen smoke (search сужава tree rows;
  `status:DONE`; clear възстановява full set; repo filter + search combined).
  Unit тестове са **отложени по решение на потребителя**.

#### Phase Status Indicators in the Main View (REQ-SW-PL-017)

От 2026-08-06 preview панелът на главния изглед (Requirements tab) показва фазова
прогресия за всяко изискване: „Архитектура ✓/✗ · Имплементация ✓/✗ · Тестове
✓/✗", изведена от проследимост полетата на изискването:

- Архитектура/Дизайн ← `Документация:`
- Имплементация ← `Код:`
- Тестване ← `Тестове:`

Семантика: празно/whitespace/„—" поле = ✗ (не е записано/направено), непразно
поле = ✓ (записано). Индикаторите са **информативни** — „✓" означава *записано*
в проследимостта, **не** *верифицирано* (непразно ≠ проверено/работещо). Статусът
остава грубия gate: `DONE` продължава да изисква пълна верификация (Qt5/Qt6
builds + unit тестове + headless smoke test) по RDD-PROCESS.md. Индикаторите не
променят статус, валидатор или lifecycle логика.

Известно следствие: изисквания DONE без `Документация:` ред (напр. PL-013/014/015)
показват „Архитектура ✗" в главния изглед — коректно според семантиката
„записано ≠ верифицирано".

- **Верификация:** Qt5/Qt6 builds. Unit тестове са **отложени по решение на
  потребителя** (standing instruction: имплементации без тестове до ново
  нареждане).

## Shared Components

- **NodeEditorWidget** — вграден в `node_editor_ide/NodeEditorWidget.{h,cpp}` (вече НЕ е отделна библиотека)

## External Libraries

- **nodeeditor** (`src/plugins/external_libs/nodeeditor/`) — Node editor library (git submodule)
- **qtrest_lib** (`src/plugins/external_libs/qtrest_lib/`) — REST API client (git submodule)

## Architecture

### Plugin Loading Flow

```
Daqster App → QPluginManager::SearchForPlugins()
  → сканира директории за .so файлове
  → QPluginLoader зарежда всеки .so
  → QPluginInterface конструктора попълва PluginDescription
  → PluginDescription PLUGIN_TYPE_NAME се използва за GUI групиране
```

### Capability Discovery

Външните плъгини се откриват чрез `QPluginManager::instances(IID)`:

```cpp
// node_editor_ide открива INodeProvider плъгини:
QObjectList providers = pm->instances(INodeProvider_IID);
for (QObject* obj : providers) {
    auto* provider = qobject_cast<INodeProvider*>(obj);
    provider->registerNodes(registry);
}
```

### Plugin Classification

Класификацията на плъгините в PluginManager GUI идва от `PluginDescription::PLUGIN_TYPE_NAME`:

| PLUGIN_TYPE_NAME | Група в PluginManager GUI |
|------------------|--------------------------|
| `"Applications"` | Applications |
| `"Node Providers"` | Node Providers |
| (празно) | Plugins |

### Plugin Interface Hierarchy

```
QPluginInterface          ← Qt plugin factory (Q_PLUGIN_METADATA)
  └── CreatePluginInternal() → QBasePluginObject

QBasePluginObject         ← Runtime plugin object (QObject)
  ├── Initialize() / DeInitialize()
  └── Може да имплементира capability интерфейси

INodeProvider             ← Standalone capability interface (Q_DECLARE_INTERFACE)
  └── registerNodes(NodeDelegateModelRegistry&)
```

**Няма базов abstract клас между QBasePluginObject и capability интерфейсите.**
Capability интерфейсите (INodeProvider) са standalone — `QBasePluginObject` наследниците ги имплементират директно чрез multiple inheritance:

```cpp
class DemoStandardNodesObject : public QBasePluginObject, public INodeProvider
{
    Q_OBJECT
    Q_INTERFACES(Daqster::INodeProvider)
};
```

## Plugin Discovery

Default search path:
```
<install_dir>/plugins/Daqster/
```

Може да добавиш допълнителни пътища чрез `QPluginManager::AddPluginsDirectory()`.

Environment variables:
- `DAQSTER_PLUGIN_DIR` — Една директория за плъгини
- `DAQSTER_PLUGIN_PATH` — Множество директории (разделени с `:`)

## Development Guide

Виж [PluginDevelopment.md](./PluginDevelopment.md) за стъпки как да създадеш нов плъгин.

## Детайлни документи

- [Plugin Development](./PluginDevelopment.md)
- [Node Editor IDE](./node_editor_ide/README.md)
- [Demo NodeEditor Nodes](./demo_nodeditor_nodes/README.md)
