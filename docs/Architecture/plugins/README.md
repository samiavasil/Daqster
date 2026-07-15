# Plugins

Родител: [Architecture Overview](../README.md) | [Documentation Index](../../index.md)

Съседни подсистеми: [Applications](../apps/README.md) | [Framework](../framework/README.md)

Плъгините добавят функционалности към Daqster приложението. Зареждат се динамично и комуникират чрез дефинирани интерфейси.

## Available Plugins

| Plugin | Локация | Тип | Описание |
|--------|---------|-----|----------|
| **Node Editor IDE** | `src/plugins/node_editor_ide/` | APPLICATION_PLUGIN | Визуален node-based редактор с вградени Audio/LLaMA нодове |
| **Demo Standard Nodes** | `src/plugins/demo_standard_nodes/` | Node Provider | INodeProvider — доставя NumberSource, NumberDisplay, Modulo нодове |
| **QtCoinTrader** | `src/plugins/QtCoinTrader/` | APPLICATION_PLUGIN | Cryptocurrency trading plugin |

### Plugin документация

Всяка основна плъгин компонента има своя документация:

- [Node Editor IDE](./node_editor_ide/README.md) — архитектура, вградени нодове, INodeProvider discovery
- [Demo Standard Nodes](./demo_standard_nodes/README.md) — INodeProvider пример, доставени нодове

## Shared Components

- **NodeEditorWidget** — вграден в `node_editor_ide/NodeEditorWidget.{h,cpp}` (вече НЕ е отделна библиотека)

## External Libraries

- **nodeeditor** (`src/plugins/external_libs/nodeeditor/`) — Node editor library (git submodule)
- **qtrest_lib** (`src/plugins/external_libs/qtrest_lib/`) — REST API client (git submodule)

## Architecture

### Plugin Loading Flow

```
Daqster App → QPluginManager::SearchForPlugins()
  → сканира директории за .so файлове
  → QPluginLoader зарежда всеки .so
  → QPluginInterface конструктора попълва PluginDescription
  → PluginDescription PLUGIN_TYPE_NAME се използва за GUI групиране
```

### Capability Discovery

Външните плъгини се откриват чрез `QPluginManager::instances(IID)`:

```cpp
// node_editor_ide открива INodeProvider плъгини:
QObjectList providers = pm->instances(INodeProvider_IID);
for (QObject* obj : providers) {
    auto* provider = qobject_cast<INodeProvider*>(obj);
    provider->registerNodes(registry);
}
```

### Plugin Classification

Класификацията на плъгините в PluginManager GUI идва от `PluginDescription::PLUGIN_TYPE_NAME`:

| PLUGIN_TYPE_NAME | Група в PluginManager GUI |
|------------------|--------------------------|
| `"Applications"` | Applications |
| `"Node Providers"` | Node Providers |
| (празно) | Plugins |

### Plugin Interface Hierarchy

```
QPluginInterface          ← Qt plugin factory (Q_PLUGIN_METADATA)
  └── CreatePluginInternal() → QBasePluginObject

QBasePluginObject         ← Runtime plugin object (QObject)
  ├── Initialize() / DeInitialize()
  └── Може да имплементира capability интерфейси

INodeProvider             ← Standalone capability interface (Q_DECLARE_INTERFACE)
  └── registerNodes(NodeDelegateModelRegistry&)
```

**Няма базов abstract клас между QBasePluginObject и capability интерфейсите.**
Capability интерфейсите (INodeProvider) са standalone — `QBasePluginObject` наследниците ги имплементират директно чрез multiple inheritance:

```cpp
class DemoStandardNodesObject : public QBasePluginObject, public INodeProvider
{
    Q_OBJECT
    Q_INTERFACES(Daqster::INodeProvider)
};
```

## Plugin Discovery

Default search path:
```
<install_dir>/plugins/Daqster/
```

Може да добавиш допълнителни пътища чрез `QPluginManager::AddPluginsDirectory()`.

Environment variables:
- `DAQSTER_PLUGIN_DIR` — Една директория за плъгини
- `DAQSTER_PLUGIN_PATH` — Множество директории (разделени с `:`)

## Development Guide

Виж [PluginDevelopment.md](./PluginDevelopment.md) за стъпки как да създадеш нов плъгин.

## Детайлни документи

- [Plugin Development](./PluginDevelopment.md)
- [Node Editor IDE](./node_editor_ide/README.md)
- [Demo Standard Nodes](./demo_standard_nodes/README.md)
