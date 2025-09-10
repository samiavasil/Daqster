# Daqster 
[Български](./README.md) | [English](./README.en.md)

Daqster е рамка (Qt5) за създаване и зареждане на плъгини, с хост приложение и примерни плъгини.

Сайт: https://samiavasil.github.io/Daqster/

## Бърз старт (CMake)

### 1) Клониране
```
git clone https://github.com/samiavasil/Daqster.git
cd Daqster
git submodule update --init --recursive
```

### 2) Конфигуриране и билд
```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

### 3) Стартиране от build
```
cd build/bin
./Daqster
```

## Инсталация и стартиране
Инсталирай (по подразбиране в `/usr/local` или в избран префикс):
```
cmake --install build --prefix ./install_dir
```
Стартиране:
```
./install_dir/bin/Daqster
```
- Библиотеките се намират автоматично чрез RPATH (`$ORIGIN/../lib`).
- Плъгините се търсят в следния ред (по приоритет):
  1. **Build директория** - `./plugins` и `../lib/daqster/plugins` (за дебъг)
  2. **Environment variables** - `DAQSTER_PLUGIN_DIR` (една директория) и `DAQSTER_PLUGIN_PATH` (множество директории, разделени с `:`)
  3. **User plugins** - `~/.local/share/daqster/plugins`
  4. **System plugins** - `/usr/lib/daqster/plugins` и `/usr/local/lib/daqster/plugins`

### Environment Variables за Plugin Discovery
- `DAQSTER_PLUGIN_DIR` - задава една директория за плъгини
- `DAQSTER_PLUGIN_PATH` - задава множество директории разделени с `:` (като PATH)

**Примери:**
```bash
# Една директория
DAQSTER_PLUGIN_DIR=/path/to/plugins ./Daqster

# Множество директории
DAQSTER_PLUGIN_PATH="/path1:/path2:/path3" ./Daqster

# И двете заедно
DAQSTER_PLUGIN_DIR=/path/to/plugins DAQSTER_PLUGIN_PATH="/path1:/path2" ./Daqster
```

## Структура на проекта
- `src/frame_work` — ядро за търсене/зареждане на плъгини
- `src/apps/Daqster` — хост приложение
- `src/plugins` — плъгини (NodeEditor, QtCoinTrader, примери/тестове)
- `src/external_libs` — външни зависимости (nodeeditor, qtrest_lib)

## Бележки
- Поддържан билд: CMake.
- Пакетиране и разширена документация могат да бъдат добавени.

