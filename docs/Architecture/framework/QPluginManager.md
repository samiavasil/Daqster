# QPluginManager

Родител: [Framework Subsystem](./README.md) | [Architecture Overview](../README.md)

**Class**: `QPluginManager`  
**Location**: `src/frame_work/base/src/QPluginManager.cpp`

## Overview

Singleton клас, който управлява откриването, зареждането и lifecycle-а на всички Daqster плъгини. Работи с `QPluginInterface` (factory) и `QBasePluginObject` (runtime).

## Responsibilities

- Сканиране на директории за .so файлове
- Зареждане чрез Qt `QPluginLoader`
- Четене на plugin metadata (`PluginDescription`)
- Управление на зависимости
- Capability discovery чрез `instances(IID)`
- UI интеграция чрез `QPluginManagerGui`

## Key API

### Search and Load

```cpp
QPluginManager::instance()->AddPluginsDirectory("/path/to/plugins");
QPluginManager::instance()->SearchForPlugins();
```

### Plugin List

```cpp
// Всички плъгини (по PluginDescription)
QList<PluginDescription> list = pm->GetPluginList(filter);

// Описание на конкретен плъгин по hash
PluginDescription desc = pm->GetPluginDescriptionByHash(hash);
```

### Capability Discovery

```cpp
// Открий всички плъгини имплементиращи даден интерфейс
QObjectList providers = pm->instances(INodeProvider_IID);
for (QObject* obj : providers) {
    auto* provider = qobject_cast<INodeProvider*>(obj);
    provider->registerNodes(registry);
}
```

`instances(IID)` използва `qt_metacast(iid)` за филтриране. Работи с **всеки** `Q_DECLAREINTERFACE` — не зависи от базов клас.

### Plugin Objects

```cpp
// Създай plugin object по hash
QBasePluginObject* obj = pm->CreatePluginObject(hash, parent);

// Enable/Disable
pm->EnableDisablePlugin(hash, true);
```

### GUI

```cpp
pm->ShowPluginManagerGui(parentWidget);
```

## Plugin Metadata

Metadata-та се съхранява в `PluginDescription` — property bag с предефинирани ключове:

| Ключ | Описание | Къде се задава |
|------|----------|----------------|
| `PLUGIN_NAME` | Име | QPluginInterface конструктор |
| `PLUGIN_VERSION` | Версия | QPluginInterface конструктор |
| `PLUGIN_TYPE` | Тип (enum) | QPluginInterface конструктор |
| `PLUGIN_TYPE_NAME` | Група в GUI | QPluginInterface конструктор |
| `PLUGIN_AUTHOR` | Автор | QPluginInterface конструктор |
| `PLUGIN_DESCRIPTION` | Кратко описание | QPluginInterface конструктор |
| `PLUGIN_HASH` | File hash | QPluginManager при зареждане |
| `PLUGIN_LOCATION` | File path | QPluginManager при зареждане |

**`PLUGIN_TYPE_NAME`** се използва от `QPluginListView` за групиране на плъгините в PluginManager GUI.

## Plugin Loading Flow

```
1. QPluginManager::SearchForPlugins()
   → сканира m_DirList за .so файлове
   → IsCandidatePluginFile() проверя дали файла съдържа "plugin" в името

2. QPluginManager::LoadPluginInterfaceObject()
   → QPluginLoader::load()
   → QPluginInterface конструктора попълва m_PluginDescryptor
   → StorePluginStateToPersistncy() запазва в QSettings

3. QPluginManager::CreatePluginObject()
   → QPluginInterface::CreatePlugin() → CreatePluginInternal()
   → Връща QBasePluginObject*
   → Добавя към m_PluginInstList
```

## Interfaces

### QPluginInterface ( frame_work )
Задължителен Qt plugin factory. Всеки Daqster плъгин имплементира този интерфейс.

```cpp
class QPluginInterface : public QObject {
    Q_OBJECT
public:
    QPluginInterface(QObject* parent = nullptr);
    virtual ~QPluginInterface();

    // Metadata
    const PluginDescription& GetPluginDescriptor() const;
    QString GetHash() const;
    bool IsEnabled() const;

    // Plugin instances
    const QList<QBasePluginObject*>& GetPluginInstances() const;

    // Factory
    QBasePluginObject* CreatePlugin(QObject* parent = nullptr);

protected:
    virtual QBasePluginObject* CreatePluginInternal(QObject* parent) = 0;

    PluginDescription m_PluginDescryptor;
    QList<QBasePluginObject*> m_PluginInstList;
};
```

### QBasePluginObject ( frame_work )
Runtime обект на плъгина.

```cpp
class QBasePluginObject : public QObject {
    Q_OBJECT
public:
    virtual bool Initialize() = 0;
protected:
    virtual void DeInitialize() = 0;
};
```

### INodeProvider ( capabilities/ )
Standalone capability interface за доставка на нодове.

```cpp
class INodeProvider {
public:
    virtual ~INodeProvider() = default;
    virtual void registerNodes(NodeDelegateModelRegistry& registry) const = 0;
};
```

## Dependency Management

- Проверява `REQUIRES_LIBRARIES` в CMake чрез `register_component()`
- Автоматично link-ва зависимости чрез `link_component_dependencies()`
- Ако зависимост липсва, плъгинът се пропуска със съобщение

## Error Handling

- Съобщения при load failure (`loader.errorString()`)
- Plugin health state: FOUNDED → IF_LOADED → HEALTHY / ILL
- Постоянно съхранение на състоянието в QSettings

## See Also

- [Plugins Overview](../plugins/README.md)
- [Framework Overview](./README.md)
- [Node Editor IDE](../../plugins/node_editor_ide/README.md) — пример за плъгин
