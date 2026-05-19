# Daqster
[Български](./README.md) | [English](./README.en.md)

Документация индекс: [docs/index.md](./docs/index.md)

Daqster е Qt-базирана платформа за модулна разработка и управление на приложения. Позволява създаване на различни типове приложения чрез плъгин архитектура с graceful shutdown, process управление и auto plugin discovery.

Сайт: https://samiavasil.github.io/Daqster/

## Бърз старт

### 1) Клониране
```bash
git clone https://github.com/samiavasil/Daqster.git
cd Daqster
git submodule update --init --recursive
```

### 2) Конфигуриране и билд

**Qt5 (по подразбиране):**
```bash
cmake -S . -B build -DUSE_QT6=OFF
cmake --build build -j
```

**Qt6:**
```bash
cmake -S . -B build -DUSE_QT6=ON
cmake --build build -j
```

**С конкретен Qt път:**
```bash
cmake -S . -B build \
  -DUSE_QT6=OFF \
  -DCMAKE_PREFIX_PATH=/path/to/Qt/5.15.2/gcc_64
cmake --build build -j
```

**Debug Build (препоръчено за разработка):**
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
```

**Release Build:**
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

За повече информация вижте [DeveloperGuide.md](./docs/development/DeveloperGuide.md).

### 3) Стартиране
```bash
cd build/bin
./Daqster
```

## Qt5/Qt6 статус

- Пълен build е валидиран и за Qt5, и за Qt6.
- Всички текущи плъгини в репото се компилират и за двете версии.
- NodeEditorPlugin е активен в Qt6 с compatibility слой за QtCharts/QtMultimedia.
- При паралелни Qt5/Qt6 build директории ползвайте отделни plugin paths, за да избегнете смесено runtime зареждане.

## Framework архитектура

Daqster използва модулна архитектура с три ключови слоя:

- **ShutdownHandler** - graceful shutdown с cross-platform signal handling (Ctrl+C, SIGTERM)
- **QProcessManager** - generic управление на child процеси с виртуални hooks
- **ApplicationsManager** - Daqster-специфична реализация с environment setup и plugin management

За подробности вижте [Framework](./docs/Architecture/framework/README.md) и [ApplicationsManager](./docs/Architecture/apps/ApplicationsManager.md).

## Plugin Discovery System

Плъгините се търсят в следния ред по приоритет:

1. Build директория - `./plugins` и `../lib/daqster/plugins` (за дебъг)
2. Environment variables - `DAQSTER_PLUGIN_DIR` и `DAQSTER_PLUGIN_PATH`
3. User плъгини - `~/.local/share/daqster/plugins`
4. System плъгини - `/usr/lib/daqster/plugins` и `/usr/local/lib/daqster/plugins`

Вижте [BuildSystemArchitecture](./docs/Architecture/BuildSystemArchitecture.md) за детайли.

## Environment Variables

**Plugin Discovery:**
- `DAQSTER_PLUGIN_DIR` - една директория за плъгини
- `DAQSTER_PLUGIN_PATH` - множество директории разделени с `:` (като PATH)

**Qt (за AppImage):**
- `LD_LIBRARY_PATH` - пътища към споделени библиотеки
- `QT_PLUGIN_PATH` - пътища към Qt плъгини
- `QML2_IMPORT_PATH` - пътища към QML модули

**XDG Directories (за AppImage):**
- `XDG_CONFIG_HOME` - конфигурационни файлове (по подразбиране: `~/.config/daqster`)
- `XDG_DATA_HOME` - данни (по подразбиране: `~/.local/share/daqster`)
- `XDG_CACHE_HOME` - кеш (по подразбиране: `~/.cache/daqster`)

За полен списък вижте [HowToDebugAppImage](./docs/development/HowToDebugAppImage.md).

## AppImage

**Локално създаване:**
```bash
./tools/create_appimage.sh
```

**С допълнителни опции:**
```bash
./tools/create_appimage.sh --help
```

За подробности вижте [tools/create_appimage.sh](./tools/create_appimage.sh).

## Документация

- [Документация индекс](./docs/index.md)
- [Architecture Overview](./docs/Architecture/README.md)
- [Development Topics](./docs/development/README.md)
- [Operations Topics](./docs/operations/README.md)
- [Porting Topics](./docs/porting/README.md)
- [Framework Subsystem](./docs/Architecture/framework/README.md)
- [Build System Architecture](./docs/Architecture/BuildSystemArchitecture.md)

## Структура на проекта

- `src/frame_work` - framework ядро (ShutdownHandler, QProcessManager)
- `src/apps/Daqster` - хост приложение с ApplicationsManager
- `src/plugins` - runtime плъгини и тестови плъгини
- `src/external_libs` - външни библиотеки
- `tools` - скриптове за build и AppImage
- `Docs` - архитектура, разработка, операции, портинг и диаграми

## Debug и диагностика

**Полезни променливи:**

- `QT_DEBUG_PLUGINS=1` - debug информация за Qt плъгини
- `QT_LOGGING_RULES="*=true"` - всички Qt debug съобщения
- `DAQSTER_PLUGIN_DIR` - пътека де плъгини (за тестване)
- `DAQSTER_PLUGIN_PATH` - множество пътеки за плъгини

**Примери:**
```bash
# Един директория за плъгини
DAQSTER_PLUGIN_DIR=/path/to/plugins ./Daqster

# Debug режим
QT_DEBUG_PLUGINS=1 QT_LOGGING_RULES="*=true" ./Daqster

# AppImage с custom пътища
LD_LIBRARY_PATH="/custom/lib:$LD_LIBRARY_PATH" ./Daqster-x86_64.AppImage
```

За подробно ръководство вижте [How To Debug AppImage](./docs/development/HowToDebugAppImage.md).
