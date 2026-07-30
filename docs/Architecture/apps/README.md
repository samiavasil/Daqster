# Applications Subsystem

Родител: [Architecture Overview](../README.md) | [Documentation Index](../../INDEX.md)

Съседни подсистеми: [Framework](../framework/README.md) | [Plugins](../plugins/README.md)

Подсистемата `apps` съдържа изпълними приложения построени върху Daqster framework-а.

## Структура

```
src/apps/
├── CMakeLists.txt          # Build configuration
└── Daqster/               # Главното приложение
    ├── ApplicationsManager  # Управление на приложения и плъгини
    ├── AppToolbar           # Toolbar UI
    ├── main.cpp             # Entry point
    └── mainwindow.ui        # Main window UI
```

## Приложения

- [**Daqster**](./Daqster.md) - Главното приложение, интегриращо всички плъгини
- [**ApplicationsManager**](./ApplicationsManager.md) - Централен мениджър за процеси и плъгини

## Как работи

1. **Startup**: `main.cpp` инициализира Qt приложението
2. **Framework Init**: Зарежда framework и създава `ApplicationsManager`
3. **Plugin Loading**: `ApplicationsManager` зарежда плъгини чрез `QPluginManager`
4. **Process Management**: Стартира и управлява процеси чрез `QProcessManager`
5. **UI**: Показва главния прозорец с toolbar и plugin виджети

## Диаграма на подсистемата

![Apps Architecture](../../diagrams/apps_architecture.svg)

[Източник: apps_architecture.puml](../../diagrams/apps_architecture.puml)

## Връзки

- [Framework Documentation](../framework/README.md) - Framework API
- [Plugin Development](../plugins/PluginDevelopment.md) - Как да създадеш plugin
- [Architecture Overview](../README.md) - Обща архитектура

## Детайлни документи

- [Daqster Application](./Daqster.md)
- [ApplicationsManager](./ApplicationsManager.md)
