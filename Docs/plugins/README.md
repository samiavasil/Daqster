# Plugins

Плъгините добавят функционалности към Daqster приложението. Зареждат се динамично и комуникират чрез дефинирани интерфейси.

## Available Plugins

- **Node Editor** (`src/plugins/node_editor/`)
- **QtCoinTrader** (`src/plugins/QtCoinTrader/`)

## How Plugins Work

- Build-ват се като Qt plugins (`.so/.dll`)
- Зареждат се чрез `QPluginManager`
- Декларират интерфейси чрез `QPluginInterface`
- Може да имат UI (widgets) и backend компоненти

## Development Guide

Виж [PluginDevelopment.md](./PluginDevelopment.md) за стъпки как да създадеш нов плъгин.

## Plugin Paths

Default search path:
```
<install_dir>/plugins/Daqster/
```

Може да добавиш допълнителни пътища чрез `QPluginManager::addSearchPath()`.
