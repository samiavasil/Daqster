# Demo Standard Nodes Plugin

Родител: [Plugins](../README.md) | [Architecture](../../Architecture/README.md)

## Преглед

Първият INodeProvider плъгин — пример за това как се създава външен доставчик на нодове. Предоставя базови математически и UI нодове.

**Локация:** `src/plugins/demo_standard_nodes/`
**Име на .so:** `libDemoStandardNodesPlugin.so`

## Архитектура

```
demo_standard_nodes/
├── DemoStandardNodesInterface.{h,cpp}   # QPluginInterface — factory
├── DemoStandardNodesInterface.json      # Plugin metadata
├── DemoStandardNodesObject.{h,cpp}      # QBasePluginObject + INodeProvider
├── NumberSourceDataModel.{h,cpp}        # Числов генератор с UI
├── NumberDisplayDataModel.{h,cpp}       # Числов дисплей
├── NumberSourceDataUi.{h,cpp,ui}        # Qt Designer widget за NumberSource
├── ModuloModel.{h,cpp}                  # Template нод: Modulo<int>, Modulo<double>
├── NumericType.h                        # Shared numeric data type
├── IntegerData.h                        # Integer NodeData
└── DecimalData.h                        # Decimal NodeData (резервен)
```

## Имплементирани интерфейси

### QPluginInterface ( frame_work )
```
DemoStandardNodesInterface → QPluginInterface
  ├── Q_PLUGIN_METADATA(IID "Daqster.PlugIn.QPluginInterface")
  ├── CreatePluginInternal() → създава DemoStandardNodesObject
  └── m_PluginDescryptor:
        PLUGIN_NAME       = "DemoStandardNodes"
        PLUGIN_TYPE       = APPLICATION_PLUGIN
        PLUGIN_TYPE_NAME  = "Node Providers"   ← групиране в PluginManager GUI
        PLUGIN_VERSION    = "0.1.0"
```

### QBasePluginObject ( frame_work )
```
DemoStandardNodesObject → QBasePluginObject
  ├── Initialize()   → return true (няма GUI — само доставя нодове)
  └── DeInitialize() → нищо
```

### INodeProvider ( standalone interface )
```
DemoStandardNodesObject → INodeProvider
  └── registerNodes(registry) →
        registry.registerModel<NumberSourceDataModel>("Sources")
        registry.registerModel<NumberDisplayDataModel>("Displays")
        registry.registerModel<ModuloModel<int>>("Operators")
        registry.registerModel<ModuloModel<double>>("Operators")
```

**INodeProvider е standalone интерфейс** — не наследява други Daqster интерфейси.
Открива се от `node_editor_ide` чрез `QPluginManager::instances(INodeProvider_IID)`.

## Доставени нодове

| Категория | Нод | Тип данни | Описание |
|-----------|-----|-----------|----------|
| Sources | NumberSourceDataModel | `double` (out), `int` (in) | Число с QLineEdit + time slider. Входът задава интервал за random генерация. |
| Displays | NumberDisplayDataModel | `double` (in) | QLabel дисплей. Няма изход. |
| Operators | ModuloModel\<int\> | `int` (in×2, out) | Modulo операция за int. Dividend / Divisor → Result. |
| Operators | ModuloModel\<double\> | `double` (in×2, out) | Modulo операция за double (fmod). |

### NumberSourceDataModel
- **Widget:** QLineEdit + QSlider (NumberSourceDataUi.ui)
- **Save/Load:** Запазва стойността в JSON (`"number"` key)
- **Behavior:** При `setInData()` с int, задава time delay за random генерация
- **Validation:** QDoubleValidator на QLineEdit

### ModuloModel\<ValueType\>
- **Template клас** — инстанцииран за `int` и `double`
- **Widget:** QComboBox с 3 елемента ("edno", "dwe", "tri") — demo placeholder
- **Validation:** Division by zero check → NodeValidationState::Error
- **caption:** `"Modulo <type>"` (динамичен от typeid)

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
