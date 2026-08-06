# Plugins

Родител: [Индекс](../INDEX.md) | [Architecture](../Architecture/README.md)

## Преглед

Този хъб събира документацията на плъгините в Daqster. Всеки основен плъгин има
свой README с архитектура, доставени нодове и зависимости. Пълният архитектурен
контекст (как плъгините се зареждат, откриват и комуникират) е в
[Architecture/plugins](../Architecture/plugins/README.md); как се пише нов плъгин —
в [PluginDevelopment](../Architecture/plugins/PluginDevelopment.md).

## Плъгини

| Плъгин | Път (код) | Описание |
|--------|-----------|----------|
| [Demo NodeEditor Nodes](./demo_nodeditor_nodes/README.md) | `src/plugins/demo_nodeditor_nodes/` | INodeProvider плъгин, който доставя Audio, Video, LLaMA, Display и Routing нодове към Node Editor IDE — основният доставчик на нодове за Daqster node editor-а |
| [Node Editor IDE](./node_editor_ide/README.md) | `src/plugins/node_editor_ide/` | Основният GUI плъгин за визуално node-based редактиране — вградени числови нодове (Source, Display, Modulo, ArithmeticLogic), споделена `NodeEditorLibrary`, динамично откриване на външни INodeProvider плъгини, QtNodes canvas |
| [QtCoinTrader](./QtCoinTrader/README.md) | `src/plugins/QtCoinTrader/` | APPLICATION_PLUGIN за cryptocurrency trading с QML UI; демонстрира интеграция с QtRest (REST API комуникация) и QtCharts (визуализация) |

## Структура

```
docs/plugins/
├── README.md                        # Този файл — хъб
├── demo_nodeditor_nodes/README.md   # Demo NodeEditor Nodes (INodeProvider доставчик)
├── node_editor_ide/README.md        # Node Editor IDE (GUI node editor)
└── QtCoinTrader/README.md           # QtCoinTrader (QML trading demo)
```

## Свързана документация

- [Architecture/plugins](../Architecture/plugins/README.md) — plugin подсистемата (QPluginInterface, INodeProvider, PluginManager)
- [PluginDevelopment](../Architecture/plugins/PluginDevelopment.md) — стъпки за създаване на нов плъгин
- [Индекс](../INDEX.md) — главна навигационна страница
