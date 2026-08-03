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

  - `DependencyGraphData` (QtCore-only) — граф + слоест layout: един възел на
    изискване, ребра `Родител:` и `Зависи от:` (case-insensitive resolution),
    Kahn's algorithm върху dependency ребрата с cycle-safe residual слой и
    `seen-set` guard (винаги терминира). Нерesolv-нати референции се записват
    като `danglingIds()` и НЕ стават ребра.
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
  (6 теста) в shared binary `requirements_manager_tests` (34/34 green на Qt5/Qt6);
  `tests/plugins/requirements_manager/test_graph_widget.{h,cpp}` — GUI binary
  `requirements_manager_gui_tests` (2 регресионни слота: `edgeFollowsNodeMove`,
  `fitsViewportAfterResize`; 4/4 green с init/cleanup, offscreen) —
  35/35 + 4/4 green на Qt5/Qt6.

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
