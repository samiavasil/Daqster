# Plugin Development

Родител: [Plugins Subsystem](./README.md) | [Architecture Overview](../README.md)

Стъпки за създаване на нов плъгин за Daqster.

## 1. Избери тип плъгин

### APPLICATION_PLUGIN — Самостоятелен плъгин с GUI
Използвай когато плъгинът има собствен прозорец/интерфейс.
Пример: `node_editor_ide`, `QtCoinTrader`

### Node Provider — Headless плъгин доставящ нодове
Имплементира `INodeProvider` и доставя `NodeDelegateModel` типове към node editor-а.
Пример: `nodeditor_nodes`

## 2. Създай скеле

### APPLICATION_PLUGIN

```
src/plugins/MyPlugin/
├── CMakeLists.txt
├── MyPluginInterface.h
├── MyPluginInterface.cpp
├── MyPluginInterface.json
├── MyPluginObject.h
└── MyPluginObject.cpp
```

### Node Provider

Node-provider plugins follow the `<plugin>/{core,gui}` directory convention
(REQ-SW-PL-060): one plugin directory, two halves. `core/` is headless (models,
engines — no `QWidget`); `gui/` holds the `QWidget`s and the `IWidgetProvider`
and links `core/`. The factory/metadata/lifecycle classes stay at the root.

```
src/plugins/my_nodes/
├── CMakeLists.txt
├── MyNodesInterface.h
├── MyNodesInterface.cpp
├── MyNodesInterface.json
├── MyNodesObject.h
├── MyNodesObject.cpp
├── core/                       # headless models/engines (no QtWidgets)
│   └── MyNodeModel.{h,cpp}
└── gui/                        # QWidgets + NodeWidgetFactory
    └── MyNodeWidget.{h,cpp}
```

Both halves are loaded as separate `.so` files by the same plugin manager. The
`QPluginManager` only scans files whose name contains `plugin` (lower-case
match), so the shared support library `src/plugins/common/` is named
`DaqsterCommonLibrary` — never `*Plugin*` — and is ignored by the scanner.

## 3. Имплементирай интерфейсите

### APPLICATION_PLUGIN

```cpp
// MyPluginInterface.h
class PLUGIN_EXPORT MyPluginInterface : public QPluginInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "Daqster.PlugIn.QPluginInterface" FILE "MyPluginInterface.json")
    Q_INTERFACES(Daqster::QPluginInterface)
public:
    MyPluginInterface(QObject* parent = nullptr);
protected:
    Daqster::QBasePluginObject* CreatePluginInternal(QObject* Parrent = nullptr) override;
};
```

```cpp
// MyPluginInterface.cpp
MyPluginInterface::MyPluginInterface(QObject* parent)
    : QPluginInterface(parent)
{
    m_PluginDescryptor.SetProperty(PLUGIN_NAME, "MyPlugin");
    m_PluginDescryptor.SetProperty(PLUGIN_TYPE, Daqster::PluginDescription::APPLICATION_PLUGIN);
    m_PluginDescryptor.SetProperty(PLUGIN_TYPE_NAME, "Applications");
    m_PluginDescryptor.SetProperty(PLUGIN_VERSION, "0.1.0");
    m_PluginDescryptor.SetProperty(PLUGIN_DESCRIPTION, "Описание на плъгина.");
    m_PluginDescryptor.SetProperty(PLUGIN_AUTHOR, "Име");
}
```

### Node Provider

```cpp
// MyNodesObject.h
class MyNodesObject : public QBasePluginObject, public INodeProvider
{
    Q_OBJECT
    Q_INTERFACES(Daqster::INodeProvider)
public:
    bool Initialize() override { return true; }  // headless
    void registerNodes(QtNodes::NodeDelegateModelRegistry& registry) const override;
protected:
    void DeInitialize() override {}
};
```

```cpp
// MyNodesObject.cpp
void MyNodesObject::registerNodes(NodeDelegateModelRegistry& registry) const
{
    registry.registerModel<MyNodeA>("Sources");
    registry.registerModel<MyNodeB>("Operators");
}
```

**Задължително:** В `MyNodesInterface.cpp` конструктора:
```cpp
m_PluginDescryptor.SetProperty(PLUGIN_TYPE_NAME, "Node Providers");
```

## 4. CMake

### APPLICATION_PLUGIN

```cmake
create_plugin(MyPlugin
    SOURCES
        MyPluginInterface.cpp MyPluginInterface.h
        MyPluginObject.cpp MyPluginObject.h
    REQUIRES_LIBRARIES
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Widgets
        frame_work
)
set_target_properties(MyPlugin PROPERTIES OUTPUT_NAME "MyPluginPlugin")
```

### Node Provider

```cmake
create_plugin(MyNodes
    SOURCES
        MyNodesInterface.cpp MyNodesInterface.h
        MyNodesObject.cpp MyNodesObject.h
        MyNodeA.cpp MyNodeA.h
        MyNodeB.cpp MyNodeB.h
    REQUIRES_LIBRARIES
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        QtNodes
        frame_work
    INCLUDE_DIRECTORIES
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${DAQSTER_SOURCE_DIR}/src/plugins/common
)
set_target_properties(MyNodes PROPERTIES OUTPUT_NAME "MyNodesPlugin")
```

**Важно:** OUTPUT_NAME на **плъгин** target трябва да съдържа `"plugin"` — `PluginDiscovery::isCandidatePluginFile()` го изисква. Обратно, споделената support библиотека `src/plugins/common/` е `DaqsterCommonLibrary` (без `plugin`) именно за да не бъде засечена като плъгин. Всяка node-provider половинка (`core`/`gui`) е отделен `create_plugin()` target с име, съдържащо `plugin`.

## 5. Plugin Metadata JSON

```json
{
    "Keys": ["Daqster.PlugIn.QPluginInterface"],
    "MetaData": {
        "Name": "MyPlugin",
        "Type": "APPLICATION_PLUGIN",
        "Version": "0.1.0",
        "Description": "Описание."
    }
}
```

## 6. Добави към корен CMakeLists.txt

```cmake
add_subdirectory(src/plugins/my_plugin)
```

## 7. Interface Reference

| Интерфейс | IID | Описание |
|-----------|-----|----------|
| `QPluginInterface` | `"Daqster.PlugIn.QPluginInterface/0.0.0"` | Задължителен. Factory за QBasePluginObject. |
| `INodeProvider` | `"org.daqster.INodeProvider/1.0"` | Capability: доставя NodeDelegateModel типове. |

### INodeProvider — standalone interface

```cpp
class INodeProvider {
public:
    virtual ~INodeProvider() = default;
    virtual void registerNodes(QtNodes::NodeDelegateModelRegistry& registry) const = 0;
};
```

- Не наследява други Daqster интерфейси
- Открива се чрез `QPluginManager::instances(INodeProvider_IID)`
- Класификацията (PLUGIN_TYPE_NAME) идва от `PluginDescription`

## 8. Debug

```bash
export QT_DEBUG_PLUGINS=1
./Daqster
```

## 9. Тестове

- Unit tests в `src/plugins/tests/`
- Виж съществуващите тестови плъгини за пример
