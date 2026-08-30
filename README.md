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

**Изисквани версии:**

- **Qt6 (PRIMARY): 6.8.3+** — кодът изисква Qt 6.8+ API: конструктора `QVideoFrame(QImage)` (Qt 6.8+) и `QImage::flipped()` (Qt 6.5+). Системният Qt 6.4.2 на Ubuntu 24.04 е ТВЪРДЕ СТАР — използвайте aqtinstall или по-нова версия на Qt.
- **Qt5 (COMPAT): 5.15.x** — поддържан за съвместимост (Qt 5.15.13 на Ubuntu 24.04, 5.15.2 локално).

**Изисквани Qt модули:** Core, Gui, Widgets, Multimedia, MultimediaWidgets, Charts, Declarative (QuickControls2), Svg

**Системни зависимости (Ubuntu 24.04):**

- Qt6 чрез aqtinstall (6.8.3) или системни пакети, където са налични
- GStreamer: `libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev` (dev) + `libgstreamer1.0-0 libgstreamer-plugins-base1.0-0 gstreamer1.0-plugins-base gstreamer1.0-plugins-good` (runtime — ЗАДЪЛЖИТЕЛНИ за QtMultimedia backend)
- OpenSSL: `libssl-dev`
- ICU: `libicu74` (Ubuntu 24.04) / `libicu70` (Ubuntu 22.04)
- Mesa GL (за offscreen/headless тестове): `libgl1-mesa-dri libegl1 libgl1 libglx-mesa0`
- CMake 3.20+, C++17 компилатор (GCC/Clang)

**Windows:**

- Qt 6.8.3 (MSVC 2022) чрез aqtinstall — модули: qtcharts, qtmultimedia (qtdeclarative и qtsvg са в base пакета)
- MSVC 2022 + Ninja

**Qt6 (по подразбиране / препоръчително):**
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=<qt6-dir> -DDAQSTER_BUILD_TESTS=ON
cmake --build build -j
```

**Qt5 (compat):**
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=<qt5-dir> -DDAQSTER_BUILD_TESTS=ON
cmake --build build -j
```

**С конкретен Qt път:**
```bash
cmake -S . -B build \
  -DCMAKE_PREFIX_PATH=/path/to/Qt/6.8.3/gcc_64
cmake --build build -j
```

**С тестове (unit + test plugins):**
```bash
cmake -S . -B build -DDAQSTER_BUILD_TESTS=ON -DDAQSTER_BUILD_TEST_PLUGINS=ON
cmake --build build -j
```

**Пускане на тестовете (headless):**
```bash
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
```

> **Забележка:** `DAQSTER_BUILD_TEST_PLUGINS` е по подразбиране OFF — включва се изрично с `-DDAQSTER_BUILD_TEST_PLUGINS=ON`.

**Debug Build (препоръчано за разработка):**
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
```

**Release Build:**
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

> **Забележка:** Qt6 се открива автоматично — `cmake/FindQtVersion.cmake` първо
> търси Qt6, после пада на Qt5. `USE_QT6` е само FORCE cache променлива, която
> указва на външните библиотеки (напр. nodeeditor) с коя Qt версия да се build-ват
> (`CMakeLists.txt:27-31`) — не е нужно да я задаваш ръчно.

За повече информация вижте [DeveloperGuide.md](./docs/development/DeveloperGuide.md).

### 3) Стартиране
```bash
cd build/bin
./Daqster
```

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
- `src/plugins/external_libs` - външни библиотеки
- `tools` - скриптове за build и AppImage
- `docs` - архитектура, разработка, операции, портинг и диаграми

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
