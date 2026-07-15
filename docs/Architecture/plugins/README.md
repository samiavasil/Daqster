# Plugins

Родител: [Architecture Overview](../README.md) | [Documentation Index](../../index.md)

Съседни подсистеми: [Applications](../apps/README.md) | [Framework](../framework/README.md)

Плъгините добавят функционалности към Daqster приложението. Зареждат се динамично и комуникират чрез дефинирани интерфейси.

## Available Plugins

- **Node Editor App** (`src/plugins/node_editor_app/`) — Базов графичен плъгин
- **QtCoinTrader** (`src/plugins/QtCoinTrader/`) — Cryptocurrency trading plugin

## Shared Components

- **NodeEditorWidget** (`src/plugins/libs/node_editor_widget/`) — Споделен GUI компонент за node-based редактори

## External Libraries

- **nodeeditor** (`src/plugins/external_libs/nodeeditor/`) — Node editor library (git submodule)
- **qtrest_lib** (`src/plugins/external_libs/qtrest_lib/`) — REST API client (git submodule)

## How Plugins Work

- Build-ват се като Qt plugins (`.so/.dll`)
- Зареждат се чрез `QPluginManager`
- Декларират интерфейси чрез `QPluginInterface`
- Може да имат UI (widgets) и backend компоненти

## Development Guide

Виж [PluginDevelopment.md](./PluginDevelopment.md) за стъпки как да създадеш нов плъгин.

## Детайлни документи

- [Plugin Development](./PluginDevelopment.md)

## Plugin Component Architecture

![Plugin Components](../diagrams/plugin_components.puml)

[PlantUML източник](../diagrams/plugin_components.puml)

## Plugin Paths

Default search path:
```
<install_dir>/plugins/Daqster/
```

Може да добавиш допълнителни пътища чрез `QPluginManager::addSearchPath()`.
