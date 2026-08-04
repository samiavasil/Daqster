# Demo NodeEditor Nodes Plugin

Родител: [Plugins](../README.md) | [Architecture](../../Architecture/README.md)

## Преглед

INodeProvider плъгин, който доставя Audio, LLaMA, Display и Routing нодове към Node Editor IDE. Това е основният доставчик на нодове за Daqster node editor-а.

**Локация:** `src/plugins/demo_nodeditor_nodes/`
**Име на .so:** `libDemoNodeEditorNodesPlugin.so`

## Архитектура

```
demo_nodeditor_nodes/
├── DemoNodeEditorNodesInterface.{h,cpp}  # QPluginInterface — factory
├── DemoNodeEditorNodesInterface.json     # Plugin metadata
├── DemoNodeEditorNodesObject.{h,cpp}     # QBasePluginObject + INodeProvider
├── Sources/
│   ├── AudioSource/                      # Аудио входни нодове
│   │   ├── AudioSourceDataModel.{h,cpp}
│   │   ├── AudioSourceDataModelUI.{h,cpp}
│   │   ├── AudioSourceConfig.{h,cpp,ui}
│   │   ├── AudioWorker.{h,cpp}
│   │   ├── AudioNodeQdevIoConnector.{h,cpp}
│   │   ├── AudioComboModel.{h,cpp}
│   │   └── node_editor.qrc
│   └── LLamaSource/                      # LLaMA AI нодове
│       ├── LLamaModelDataModel.{h,cpp}
│       ├── ConsoleDataModel.{h,cpp}
│       └── ChatBaseWidget.{h,cpp}
├── Displays/
│   ├── AudioDisplay/
│   │   └── AudioDisplayModel.{h,cpp}
│   └── GenericDisplay/
│       └── GenericDisplayNode.{h,cpp}
└── Routing/
    ├── Demux/
    │   └── DemuxNode.{h,cpp}
    └── Mux/
        └── MuxNode.{h,cpp}
```

## Имплементирани интерфейси

### QPluginInterface ( frame_work )
```
DemoNodeEditorNodesInterface → QPluginInterface
  ├── Q_PLUGIN_METADATA(IID "Daqster.PlugIn.QPluginInterface")
  ├── CreatePluginInternal() → създава DemoNodeEditorNodesObject
  └── m_PluginDescryptor:
        PLUGIN_NAME       = "DemoNodeEditorNodes"
        PLUGIN_TYPE       = APPLICATION_PLUGIN
        PLUGIN_TYPE_NAME  = "Node Providers"   ← групиране в PluginManager GUI
        PLUGIN_VERSION    = "0.2.0"
```

### QBasePluginObject ( frame_work )
```
DemoNodeEditorNodesObject → QBasePluginObject
  ├── Initialize()   → return true (няма GUI — само доставя нодове)
  └── DeInitialize() → логира унищожаване
```

### INodeProvider ( standalone interface )
```
DemoNodeEditorNodesObject → INodeProvider
  └── registerNodes(registry) →
        registry.registerModel<AudioDisplayModel>("Displays")
        registry.registerModel<GenericDisplayNode>("Displays")
        registry.registerModel<DemuxNode>("Routing")
        registry.registerModel<MuxNode>("Routing")
        registry.registerModel<AudioSourceDataModel>("Sources")
        registry.registerModel<LLamaModelDataModel>("LLama")
        registry.registerModel<ConsoleDataModel>("LLama")
```

**INodeProvider е standalone интерфейс** — не наследява други Daqster интерфейси.
Открива се от `node_editor_ide` чрез `QPluginManager::instances(INodeProvider_IID)`.

## Доставени нодове

### Sources
| Нод | Категория | Описание |
|-----|-----------|----------|
| AudioSourceDataModel | Sources | Аудио вход с QWidget UI панел и QDeviceIOConnector |

### Displays
| Нод | Категория | Описание |
|-----|-----------|----------|
| AudioDisplayModel | Displays | Аудио дисплей за визуализация на аудио данни |
| GenericDisplayNode | Displays | Универсален дисплей за generic данни |

### Routing
| Нод | Категория | Описание |
|-----|-----------|----------|
| DemuxNode | Routing | Demultiplexer — разделя един input stream на множество output-и |
| MuxNode | Routing | Multiplexer — комбинира множество input streams в един output |

### LLama
| Нод | Категория | Описание |
|-----|-----------|----------|
| LLamaModelDataModel | LLama | LLaMA модел — AI генерация на текст |
| ConsoleDataModel | LLama | Конзола за показване на AI отговори |

## Зависимости

Plugin-ът изисква следните Qt модули и библиотеки:
- **Qt::Core, Qt::Gui** — основни Qt модули
- **Qt::Charts** — за графична визуализация
- **Qt::Multimedia** — за аудио функционалност
- **Qt::Network** — за мрежова комуникация (LLaMA API)
- **Qt::OpenGL** — за rendering
- **QtNodes** — node editor library
- **NodeEditorLibrary** — споделена библиотека за node editor
- **frame_work** — Daqster core framework

## Вид на плъгина

Това е **HEADLESS** плъгин — няма GUI. `Initialize()` връща `true` без да създава прозорец.
Целта му е единствено да регистрира нодове в nodeeditor registry-я.

Такива плъгини се откриват от `node_editor_ide` чрез:
```cpp
QObjectList providers = pm->instances(INodeProvider_IID);
```

## Как да добавиш нов INodeProvider плъгин

1. Създай клас наследяващ `QBasePluginObject` + `INodeProvider`
2. Имплементирай `registerNodes()` — registriрай моделите
3. В QPluginInterface конструктора задай `PLUGIN_TYPE_NAME` (за GUI групиране)
4. В CMakeLists.txt използвай `create_plugin()` с `REQUIRES_LIBRARIES` → `QtNodes`, `frame_work`

## Свързана документация

- [IPluginInterface](../../Architecture/framework/README.md) — как плъгините се откриват
- [INodeProvider](../../Architecture/plugins/README.md) — capability discovery mechanism
- [Node Editor IDE](../node_editor_ide/README.md) — потребителят на този плъгин
