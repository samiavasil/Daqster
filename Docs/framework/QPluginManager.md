# QPluginManager

**Class**: `QPluginManager`  
**Location**: `src/frame_work/base/src/QPluginManager.cpp`

## Overview

Управлява динамично зареждане на плъгини, тяхната мета-информация, зависимости и lifecycle. Осигурява интерфейси за плъгини чрез `QPluginInterface`.

## Responsibilities

- Зареждане/разтоварване на плъгини (Qt `QPluginLoader`)
- Четене на plugin metadata
- Управление на зависимости
- Достъп до plugin интерфейси
- UI интеграция чрез `QPluginManagerGui`

## Plugin Metadata

Plugin-ите имат мета-информация (JSON/Qt мета-данни):
- `name`: Име на плъгина
- `version`: Версия
- `dependencies`: други плъгини/библиотеки
- `capabilities`: предоставяни интерфейси/функции

## Loading

```cpp
QPluginLoader loader(pluginPath);
QObject* plugin = loader.instance();
if (plugin) {
    // Query interface
    auto iface = qobject_cast<QPluginInterface*>(plugin);
    if (iface) {
        // Register plugin
        registerPlugin(iface);
    }
}
```

## Interfaces

### QPluginInterface

Общ интерфейс за плъгини:
```cpp
class QPluginInterface {
public:
    virtual ~QPluginInterface() {}
    virtual QString name() const = 0;
    virtual QString version() const = 0;
    virtual void initialize() = 0;
    virtual void shutdown() = 0;
};
```

## Dependency Management

- Проверява `dependencies` в мета-информацията
- Зарежда в правилен ред
- Fail-ва при липсващи зависимости

## GUI Integration

`QPluginManagerGui` показва списък с плъгини, статус, бутони за enable/disable.

## Error Handling

- Съобщения при load failure (`loader.errorString()`)
- Логиране на dependency проблеми

## Usage

```cpp
QPluginManager manager;
manager.addSearchPath("plugins/Daqster");
manager.loadAll();

auto plugin = manager.getPlugin("NodeEditor");
if (plugin) {
    plugin->initialize();
}
```

## See Also

- [Plugin System](./PluginSystem.md)
- [ApplicationsManager](../apps/ApplicationsManager.md)
- [Framework Overview](./README.md)
