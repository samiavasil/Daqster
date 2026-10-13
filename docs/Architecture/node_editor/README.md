# Node Editor (подсистема)

Родител: [Architecture](../README.md) | [Индекс](../../index.md)
Виж още: [Core/GUI Split](../core-gui-split.md) · [Plugins](../plugins/README.md) ·
[Flow формат](../formats/flow-format.md) · [REST/remote control](../rest-api-remote-control.md)

Тази област събира архитектурната документация на **Node Editor** — визуалния
node-based редактор, с който се изграждат потоци от данни (audio / video / DAQ /
LLM / routing). Тя е самостоятелна и се реферира от главната документация на
проекта ([Architecture](../README.md), [Индекс](../../index.md)).

## Какво представлява

Node Editor-ът е изграден върху външната библиотека **QtNodes** (git submodule
`src/plugins/external_libs/nodeeditor/`), но Daqster я обвива в собствена
архитектура с ясно разделение **model (headless) / widgets (GUI)**:

- **Моделът** (граф, нодове, връзки, сериализация) е widget-free и работи и без GUI.
- **Widget-ите** (canvas, конфигурационни панели, дисплеи) живеят отделно и се
  добавят само в GUI режим.

Това разделение е същото, което важи за целия проект — виж
[Core/GUI Split](../core-gui-split.md).

## Компоненти

```
┌───────────────────────────────────────────────────────────────────────────┐
│  external_libs/nodeeditor  (git submodule)                                 │
│  ├── QtNodesCore  — model-only canvas (QtCore + QtGui, БЕЗ QtWidgets)      │
│  └── QtNodes      — GUI половина (QtWidgets), линква QtNodesCore           │
└───────────────────────────────────────────────────────────────────────────┘
            ▲                                       ▲
            │                                       │
┌───────────┴───────────────────┐   ┌───────────────┴───────────────────────┐
│  node_editor_ide  (плъгин)     │   │  nodeditor_nodes  (плъгин, 2 половини)│
│  NodeEditorIde                 │   │  core → INodeProvider (модели)        │
│  IDE shell: QMainWindow +      │   │  gui  → IWidgetProvider (widget-и)    │
│  NodeEditorWidget              │   │                                       │
│  – открива INodeProvider-и     │   │  доставя Audio/Video/DAQ/LLM/... нодове│
│  – НЕ регистрира свои нодове   │   │                                       │
│  + NodeSupport                 │   └───────────────────────────────────────┘
│    (libNodeEditorLibrary.so)   │
└────────────────────────────────┘
            ▲                                       ▲
            └──────────────┬────────────────────────┘
                           │
     src/plugins/common/capabilities/  (INodeProvider, IWidgetProvider,
                                        INodeConfigSchema, IStartable/IStoppable)
```

| Компонент | Път (код) | Артефакт | Роля |
|-----------|-----------|----------|------|
| **QtNodesCore** | `src/plugins/external_libs/nodeeditor/` | библиотека | Model-only canvas: граф, нодове, връзки, сериализация (QtCore + QtGui, без QtWidgets) |
| **QtNodes** | `src/plugins/external_libs/nodeeditor/` | библиотека | GUI половина: `QGraphicsView` canvas, embedded widgets (QtWidgets); линква QtNodesCore |
| **Node Editor IDE** | `src/plugins/node_editor/node_editor_ide/` | `libNodeEditorIde.so` | IDE shell плъгин (`NodeEditorIde`): `QMainWindow` + `NodeEditorWidget`; **открива** външни `INodeProvider` плъгини |
| **NodeSupport** | `src/plugins/node_editor/node_editor_ide/NodeSupport/` | `libNodeEditorLibrary.so` | Споделена поддръжка: типове, конектори, дисплей модели, decoders, threading |
| **Nodeditor Nodes** | `src/plugins/node_editor/nodeditor_nodes/{core,gui}/` | `libNodeditorNodesCorePlugin.so` + `libNodeditorNodesGuiPlugin.so` | Основният **доставчик на нодове** — `core` (модели, `INodeProvider`), `gui` (widget-и, `IWidgetProvider`) |
| **Capabilities** | `src/plugins/common/capabilities/` | header-only интерфейси | Договорите между хоста и плъгините: `INodeProvider`, `IWidgetProvider`, `INodeConfigSchema`, `IStartable`/`IStoppable`, `IRuntimeHost` |

> **Забележка (REQ-SW-PL-060/061):** plugin директориите са `snake_case`, C++
> класовете/CMake target-ите са `PascalCase`. `node_editor_ide` + `nodeditor_nodes`
> живеят под общата група `src/plugins/node_editor/` (групираща папка без
> `CMakeLists.txt` — root-ът изброява листата).

## Как се свързват нещата

1. **IDE shell** (`NodeEditorIde`) вдига прозореца и canvas-а (чрез `QtNodes`) и създава
   `NodeDelegateModelRegistry`.
2. Той пита `QPluginManager` за всички заредени `INodeProvider` инстанции
   (`instances(INodeProvider_IID)`) и вика `registerNodes(registry)` върху всяка.
3. **`NodeditorNodesCore`** регистрира моделите на нодовете (widget-free) под
   категории `Sources/*`, `Displays/*`, `Operators/*`, `Sinks/*`, `Processing/*`,
   `AI/LLM`, `Routing` — пълният каталог е в [nodes.md](nodes.md).
4. За да се покажат настройки/дисплеи, **`NodeditorNodesGui`** доставя widget-ите чрез
   `IWidgetProvider` (`NodeWidgetFactory`). В headless режим този плъгин не се зарежда
   и нодовете остават конфигурируеми само програмно (през `load()`/`save()`).
5. **Runtime host-овете** (`RuntimeHostHeadlessPlugin` / `RuntimeHostGuiPlugin`, виж
   [runtime-host](../../plugins/README.md)) вдигат съответния режим и са
   *консуматори* на node editor-а — затова **не** са част от тази група.

## Документи в тази област

| Документ | Съдържание |
|----------|-----------|
| [nodes.md](nodes.md) | Каноничният каталог на нодовете + категориите (какво се регистрира) |
| [adr/](adr/README.md) | Architecture Decision Records (stream routing, generic numeric port, plugin separation) |
| [../plugins/README.md](../plugins/README.md) | Plugin-подсистемата (QPluginInterface, discovery, capabilities) |
| [../formats/flow-format.md](../formats/flow-format.md) | `.flow` JSON форматът (сцени/графове) |
| [../core-gui-split.md](../core-gui-split.md) | Model/GUI разделянето и конвенцията `<plugin>/{core,gui}` |
| [../rest-api-remote-control.md](../rest-api-remote-control.md) | Управление на графа отдалечено (REST + WebSocket) |

## Свързана документация

- [Node Editor IDE (плъгин)](../../plugins/node_editor_ide/README.md)
- [Nodeditor Nodes (плъгин)](../../plugins/nodeditor_nodes/README.md)
- [Capabilities каталог](../capabilities.md)
- [Runtime host плъгини](../../plugins/README.md)
