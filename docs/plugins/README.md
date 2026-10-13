# Plugins

Родител: [Индекс](../index.md) | [Architecture](../Architecture/README.md)

## Преглед

Този хъб събира документацията на плъгините в Daqster. Всеки основен плъгин има
свой README с архитектура, доставени нодове и зависимости. Пълният архитектурен
контекст (как плъгините се зареждат, откриват и комуникират) е в
[Architecture/plugins](../Architecture/plugins/README.md); как се пише нов плъгин —
в [PluginDevelopment](../Architecture/plugins/PluginDevelopment.md).

## Плъгини

| Плъгин | Път (код) | Описание |
|--------|-----------|----------|
| [Nodeditor Nodes](./nodeditor_nodes/README.md) | `src/plugins/node_editor/nodeditor_nodes/` | INodeProvider (**core**, widget-free модели) + IWidgetProvider (**gui**, widget-и) — основният доставчик на Audio, Video, DAQ, LLaMA, Display и Routing нодове |
| [Node Editor IDE](./node_editor_ide/README.md) | `src/plugins/node_editor/node_editor_ide/` | IDE shell плъгин: `QMainWindow` + `NodeEditorWidget` (QtNodes canvas), открива външни `INodeProvider` плъгини; съдържа споделената `NodeSupport` (`libNodeEditorLibrary.so`) |
| Runtime Host Headless | `src/plugins/runtime_host/headless/` | Headless runtime плъгин (`RuntimeHostHeadlessPlugin`) — `IRuntimeHost`, вдига граф без GUI |
| Runtime Host GUI | `src/plugins/runtime_host/gui/` | GUI runtime shell (`RuntimeHostGuiPlugin`) — `IRuntimeHost`, вдига графа с прозорци/canvas |
| [QtCoinTrader](./qt_coin_trader/README.md) | `src/plugins/qt_coin_trader/` | APPLICATION_PLUGIN за cryptocurrency trading с QML UI; демонстрира интеграция с QtRest (REST API комуникация) и QtCharts (визуализация) |
| Requirements Manager | `src/plugins/requirements_manager/` | APPLICATION_PLUGIN за управление на изискванията (RDD) с GUI |

> **Node Editor документацията** вече живее в собствена област:
> [Architecture/node_editor](../../Architecture/node_editor/README.md).

## Структура

```
docs/plugins/
├── README.md                       # Този файл — хъб
├── nodeditor_nodes/README.md       # Nodeditor Nodes (INodeProvider/IWidgetProvider доставчик)
├── node_editor_ide/README.md       # Node Editor IDE (IDE shell + NodeSupport)
├── qt_coin_trader/README.md        # QtCoinTrader (QML trading demo)
└── requirements_manager/README.md  # Requirements Manager (RDD UI)
```

## Свързана документация

- [Architecture/plugins](../Architecture/plugins/README.md) — plugin подсистемата (QPluginInterface, INodeProvider, PluginManager)
- [PluginDevelopment](../Architecture/plugins/PluginDevelopment.md) — стъпки за създаване на нов плъгин
- [Индекс](../index.md) — главна навигационна страница
