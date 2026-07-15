# Node Editor IDE Plugin

Родител: [Plugins](../README.md) | [Architecture](../../Architecture/README.md)

## Преглед

Основният GUI плъгин за визуално node-based редактиране. Предоставя:
- Вградени Audio и LLaMA нодове
- Динамично откриване на външни INodeProvider плъгини
- QtNodes canvas с поддръжка на кръгови връзки

**Локация:** `src/plugins/node_editor_ide/`
**Име на .so:** `libNodeEditorPluginIde.so`

## Архитектура

```
node_editor_ide/
├── NodeEditorIdeInterface.{h,cpp}   # QPluginInterface — factory
├── NodeEditorIdeInterface.json      # Plugin metadata
├── NodeEditorIdeObject.{h,cpp}      # QBasePluginObject — runtime
├── NodeEditorWidget.{h,cpp}         # Вграден QWidget (бивш node_editor_widget)
├── ChatGraphModel.h                 # Loop-enabled DataFlowGraphModel
└── BuiltInNodes/                    # Вградени нодове
    ├── Audio/                       # AudioSource, AudioCombo, AudioWorker...
    ├── Library/                     # QDevIoDisplay, XYSeries, AudioFrameDecoder...
    ├── Examples/                    # Converters, DecimalData, IntegerData
    ├── ThreadPull/                  # EventThreadPull
    └── LLama/                       # LLamaModelDataModel, ConsoleDataModel, ChatBaseWidget
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
        PLUGIN_VERSION    = "0.2.0"
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

## Вградени нодове (Built-in)

Регистрирани директно в `registerBuiltInNodes()`:

| Категория | Нод | Описание |
|-----------|-----|----------|
| Sources | AudioSourceDataModel | Аудио вход |
| Displays | QDevIoDisplayModel | Qt I/O дисплей |
| LLaMA | LLamaModelDataModel | LLaMA AI модел |
| LLaMA | ConsoleDataModel | Конзолен изход |

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

Външните INodeProvider плъгини (напр. `demo_standard_nodes`) се откриват автоматично чрез `QPluginManager::instances(INodeProvider_IID)` и техните нодове се добавят към registry-я преди `buildCanvas()`.

## Известни зависимости

| Зависимост | Тип | Описание |
|-----------|-----|----------|
| QtNodes | External lib | nodeeditor submodule — `src/plugins/external_libs/nodeeditor/` |
| Qt::Core, Gui, Widgets | Qt | Основни Qt модули |
| Qt::Multimedia, Charts, OpenGL | Qt | За Audio и LLaMA нодове |
| Qt::Network | Qt | За REST/Llama通信 |
| frame_work | Internal | QPluginManager, QBasePluginObject, QPluginInterface |

## Свързана документация

- [IPluginInterface](../../Architecture/framework/README.md) — как плъгините се откриват и зареждат
- [INodeProvider](../../Architecture/plugins/README.md#capability-discovery) — как външни плъгини доставят нодове
- [PluginManager GUI](../../Architecture/framework/QPluginManager.md) — как плъгините се показват в UI
