# Daqster 
[Български](./README.md) | [English](./README.en.md)

Daqster е рамка (Qt5) за създаване и зареждане на плъгини, с хост приложение и примерни плъгини.

Сайт: https://samiavasil.github.io/Daqster/

## Бърз старт (CMake)

### 1) Клониране
```bash
 git clone https://github.com/samiavasil/Daqster.git
 cd Daqster
 git submodule update --init --recursive
```

### 2) Конфигуриране и билд

#### Debug Build (препоръчително за разработка)
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
```

#### Release Build (за production)
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

### 3) Стартиране от build
```bash
cd build/bin
./Daqster 
```

### 4) Създаване на AppImage (локално)

#### Използване на универсалния скрипт
```bash
# Локален режим (по подразбиране)
./tools/create_appimage.sh

# С custom параметри
./tools/create_appimage.sh --qt-dir /path/to/qt --source-dir /path/to/build

# Помощ
./tools/create_appimage.sh --help
```

#### Ръчно създаване на AppImage
```bash
# 1. Инсталирай в staging директория
cmake --install build --prefix ./stage

# 2. Създай AppImage
./tools/create_appimage.sh --mode ci --source-dir ./stage
```

**Резултат:** `Daqster-x86_64.AppImage` в project root директорията

## Инсталация и стартиране

### Инсталация
```bash
# Инсталирай в custom директория
cmake --install build --prefix ./install_dir

# Или в системна директория
sudo cmake --install build --prefix /usr/local
```

### Стартиране
```bash
# От install директория
./install_dir/bin/Daqster

# Или от build директория (за разработка)
cd build/bin
./Daqster
```

### Plugin Discovery System

Плъгините се търсят в следния ред (по приоритет):

1. **Build директория** - `./plugins` и `../lib/daqster/plugins` (за дебъг)
2. **Environment variables** - `DAQSTER_PLUGIN_DIR` и `DAQSTER_PLUGIN_PATH`
3. **User plugins** - `~/.local/share/daqster/plugins`
4. **System plugins** - `/usr/lib/daqster/plugins` и `/usr/local/lib/daqster/plugins`

### Environment Variables

#### Plugin Discovery
- `DAQSTER_PLUGIN_DIR` - задава една директория за плъгини
- `DAQSTER_PLUGIN_PATH` - задава множество директории разделени с `:` (като PATH)

#### Qt Environment (за AppImage)
- `LD_LIBRARY_PATH` - пътища към споделени библиотеки
- `QML2_IMPORT_PATH` - пътища към QML модули
- `QT_PLUGIN_PATH` - пътища към Qt плъгини
- `QT_QPA_PLATFORM_PLUGIN_PATH` - пътища към platform плъгини

#### XDG Directories (за AppImage)
- `XDG_CONFIG_HOME` - конфигурационни файлове (по подразбиране: `~/.config/daqster`)
- `XDG_DATA_HOME` - данни (по подразбиране: `~/.local/share/daqster`)
- `XDG_CACHE_HOME` - кеш (по подразбиране: `~/.cache/daqster`)

#### Debug Environment
- `QT_DEBUG_PLUGINS=1` - включва debug информация за Qt плъгини
- `QT_LOGGING_RULES="*=true"` - включва всички Qt debug съобщения
- `DAQSTER_DEBUG=1` - включва Daqster-специфични debug съобщения

**Примери за използване:**
```bash
# Една директория за плъгини
DAQSTER_PLUGIN_DIR=/path/to/plugins ./Daqster

# Множество директории за плъгини
DAQSTER_PLUGIN_PATH="/path1:/path2:/path3" ./Daqster

# Debug режим
QT_DEBUG_PLUGINS=1 QT_LOGGING_RULES="*=true" ./Daqster

# AppImage с custom пътища
LD_LIBRARY_PATH="/custom/lib:$LD_LIBRARY_PATH" ./Daqster-x86_64.AppImage
```

## Структура на проекта
- `src/frame_work` — ядро за търсене/зареждане на плъгини
- `src/apps/Daqster` — хост приложение
- `src/plugins` — плъгини (NodeEditor, QtCoinTrader, примери/тестове)
- `src/external_libs` — външни зависимости (nodeeditor, qtrest_lib)
- `tools/` — инструменти за билд и пакетиране
  - `create_appimage.sh` — универсален скрипт за създаване на AppImage
  - `Build_AppImage/` — директория за локални AppImage билдове
- `Docs/` — документация
  - `Architecture.md` — архитектура на проекта
  - `HowToDebugAppImage.md` — ръководство за дебъгване на AppImage

## Build Types

### Debug Build
- **Цел:** Разработка и дебъгване
- **Особености:** Debug символи, оптимизации изключени, подробни логове
- **Команда:** `cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug`

### Release Build
- **Цел:** Production и performance
- **Особености:** Оптимизации включени, debug символи изключени
- **Команда:** `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release`

## AppImage Support

### Локално създаване
```bash
# Автоматично (препоръчително)
./tools/create_appimage.sh

# С custom параметри
./tools/create_appimage.sh --qt-dir /path/to/qt --source-dir /path/to/build
```

### CI/CD
AppImage се създава автоматично при всеки push/PR в GitHub Actions:
- **Release AppImage** - оптимизиран за разпространение
- **Debug AppImage** - с debug символи за дебъгване

### Използване на AppImage
```bash
# Стартиране
./Daqster-x86_64.AppImage

# С аргументи (за стартиране на конкретен plugin)
./Daqster-x86_64.AppImage QtCoinTrader

# Debug режим
QT_DEBUG_PLUGINS=1 ./Daqster-x86_64.AppImage
```

## Бележки
- **Build система:** CMake 3.16+
- **Qt версия:** 5.15.2 (локално), 5.15.2+ (CI)
- **AppImage:** Универсален Linux пакет, работи на всички дистрибуции
- **Plugin система:** Динамично зареждане с автоматично откриване

