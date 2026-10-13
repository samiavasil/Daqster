# Node Editor IDE Plugin

Родител: [Plugins](../README.md) | [Architecture](../../Architecture/README.md) | [Node Editor](../../Architecture/node_editor/README.md)

## Преглед

Основният GUI плъгин за визуално node-based редактиране. Предоставя:
- Вградени числови нодове (Source, Display, Modulo, ArithmeticLogic)
- Споделена библиотека (`NodeEditorLibrary.so`) за display, connectors, threading
- Динамично откриване на външни INodeProvider плъгини
- QtNodes canvas с поддръжка на кръгови връзки

**Локация:** `src/plugins/node_editor/node_editor_ide/`
**Име на .so:** `libNodeEditorIde.so` (REQ-SW-PL-051: преименувано от `libNodeEditorPluginIde.so`)

> **Бележка:** името `libNodeEditorIde.so` не съдържа подниза „plugin", затова
> `PluginDiscovery::isCandidatePluginFile()` го приема по вградената
> `Q_PLUGIN_METADATA`, не по име — виж
> [PluginDevelopment](../../Architecture/plugins/PluginDevelopment.md).

## Архитектура

```
node_editor_ide/
├── NodeEditorIdeInterface.{h,cpp}   # QPluginInterface — factory
├── NodeEditorIdeInterface.json      # Plugin metadata
├── NodeEditorIdeObject.{h,cpp}      # QBasePluginObject — runtime
├── NodeEditorWidget.{h,cpp}         # Вграден QWidget (бивш node_editor_widget)
├── NodeSupport/                     # Споделена библиотека (NodeEditorLibrary.so)
│   ├── types/                       # GenericNumericTypes, ChatGraphModel, IStreamDecoder...
│   ├── connectors/                  # GenericQDevIoConnector, NodeDataModelToQIODeviceConnector
│   ├── display/                     # QDevIoDisplayModelObsolete, XYSeriesIODevice, AudioCompat
│   ├── threading/                   # EventThreadPull
│   ├── decoders/                    # AudioFrameDecoder
│   └── (docs: ADR-001..003 вече в docs/Architecture/node_editor/adr/)
└── node_editor.qrc                  # Ресурси (икона)
```

## Имплементирани интерфейси

### QPluginInterface ( frame_work )
Задължителен за всички Daqster плъгини. Виж [Framework/QPluginInterface](../../Architecture/framework/README.md).

```
NodeEditorIdeInterface → QPluginInterface
  ├── Q_PLUGIN_METADATA(IID "Daqster.PlugIn.QPluginInterface")
  ├── CreatePluginInternal() → създава NodeEditorIdeObject
  └── m_PluginDescryptor:
        PLUGIN_NAME       = "NodeEditorIDE"
        PLUGIN_TYPE       = APPLICATION_PLUGIN
        PLUGIN_TYPE_NAME  = "Applications"    ← използва се за групиране в PluginManager GUI
        PLUGIN_VERSION    = "0.3.2"
```

### QBasePluginObject ( frame_work )
Runtime обект на плъгина. Виж [Framework/QBasePluginObject](../../Architecture/framework/README.md).

```
NodeEditorIdeObject → QBasePluginObject
  ├── Initialize()  → създава QMainWindow, NodeEditorWidget, регистрира нодове
  ├── DeInitialize() → deleteLater() на прозореца
  └── SetName()     → задава window title
```

### INodeProvider — НЕ
NodeEditorIdeObject **не** имплементира INodeProvider. Той е **потребител** на INodeProvider — открива външни доставчици и извиква `registerNodes()` върху тях.

## Вградени нодове (демо набори)

> **ВАЖНО:** редът по-долу е исторически — бившите `BuiltInNodes/Sources|Displays|Operators`
> (NumberSource, NumberDisplay, Modulo, ArithmeticLogic) са преместени в
> `nodeditor_nodes/core` при REQ-SW-PL-051 и се регистрират в
> `NodeditorNodesCoreObject::registerNodes()`. `NodeEditorIdeObject` не
> регистрира собствени нодове — той **открива** външни `INodeProvider` плъгини.

| Категория | Нод | Описание |
|-----------|-----|----------|
| Sources | NumberSourceDataModel | Числов генератор (int/double, random, timer) |
| Displays | NumberDisplayDataModel | Числов дисплей (int/double) |
| Operators | ModuloModel | Модуло операция (int/double) |
| Operators | ArithmeticLogicModel | Аритметичен/логичен израз (2–8 входа, C++ expression) |

## Динамично откриване на нодове

```cpp
void NodeEditorIdeObject::discoverAndRegisterExternalNodes() {
    QObjectList providers = pm->instances(INodeProvider_IID);
    for (QObject* obj : providers) {
        auto* provider = qobject_cast<INodeProvider*>(obj);
        provider->registerNodes(*registry);
    }
}
```

Външните INodeProvider плъгини (напр. `nodeditor_nodes`) се откриват автоматично чрез `QPluginManager::instances(INodeProvider_IID)` и техните нодове се добавят към registry-я преди `buildCanvas()`.

## Известни зависимости

| Зависимост | Тип | Описание |
|-----------|-----|----------|
| NodeEditorLibrary | Shared lib | Споделена библиотека (display, connectors, threading, types) |
| QtNodes / QtNodesCore | External lib | nodeeditor submodule — `src/plugins/external_libs/nodeeditor/` (`QtNodesCore` = model-only, QtCore+QtGui; `QtNodes` = QtWidgets canvas, линква QtNodesCore) |
| Qt::Core, Gui, Widgets | Qt | Основни Qt модули |
| Qt::Multimedia, Charts, OpenGL | Qt | За display и audio нодове |
| Qt::Network | Qt | За REST通信 |
| frame_work / FrameworkCore | Internal | QPluginManager, QBasePluginObject, QPluginInterface |

## Свързана документация

- [Node Editor (подсистема)](../../Architecture/node_editor/README.md) — цялостната архитектура на node editor-а
- [IPluginInterface](../../Architecture/framework/README.md) — как плъгините се откриват и зареждат
- [INodeProvider](../../Architecture/plugins/README.md#capability-discovery) — как външни плъгини доставят нодове
- [PluginManager GUI](../../Architecture/framework/QPluginManager.md) — как плъгините се показват в UI
