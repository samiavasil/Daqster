[Български](./BuildSystemArchitecture.md) | [English](./BuildSystemArchitecture.en.md)

# Архитектура на build системата на Daqster

## Общ преглед

Build системата на Daqster е базирана на CMake шаблонни функции, които автоматично управляват зависимостите и условното компилиране на компоненти (приложения, plugins, библиотеки) според наличните Qt модули, външни библиотеки и пакети.

### Ключови характеристики (v2.1)

- **Декларативен синтаксис**: Всичко е в извикването на `create_plugin()`
- **Автоматична проверка на зависимости**: Проверява се преди компилация
- **Ранно прекратяване**: Компонентите с липсващи зависимости не се създават
- **Ясни параметри**: `INCLUDE_DIRECTORIES`, `COMPILE_DEFINITIONS`, `LINK_LIBRARIES`, `INSTALL_RPATH`
- **Qt5/Qt6 съвместимост**: Автоматично адаптиране според Qt версията
- **Самостоятелни компоненти**: Всеки компонент декларира собствените си зависимости
- **Без CMake грешки**: Target командите се извикват само за включените компоненти

## Ключови принципи

### 1. Създаване на компоненти чрез шаблони
Всеки тип компонент (app, plugin, library) се създава чрез специализирана template функция, която:
- Автоматично проверява dependencies
- Автоматично линква необходимите библиотеки
- Настройва правилните include directories
- Конфигурира install rules
- Управлява RPATH за runtime linking

### 2. Самостоятелни компоненти
Всеки компонент декларира своите dependencies в собствения си `CMakeLists.txt` файл, без нужда от централизирана регистрация.

### 3. Динамично откриване на Qt модули
Qt модулите се откриват динамично само когато са необходими на компонент, вместо предварително да се търсят всички възможни модули.

## Шаблони за компоненти

### 1. Приложения - `create_application()`
```cmake
create_application(Daqster
    SOURCES
        main.cpp
        ApplicationsManager.cpp
        AppToolbar.cpp
        # ... други файлове
    REQUIRES_LIBRARIES
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Gui
        frame_work
)
```

**Характеристики:**
- Създава executable
- Автоматично линква dependencies
- Install в `bin/`
- Подходящ RPATH за намиране на библиотеки

### 2. Plugins - `create_plugin()`
```cmake
create_plugin(QtCoinTraderPlugin
    SOURCES
        QtCoinTraderInterface.cpp
        QtCoinTraderPluginObject.cpp
        utils/RandData.cpp
        # ... други файлове
    REQUIRES_LIBRARIES
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        Qt${QT_VERSION_MAJOR}::Qml
        Qt${QT_VERSION_MAJOR}::Network
        Qt${QT_VERSION_MAJOR}::WebSockets
        qtrest_lib
        OpenSSL::SSL
        OpenSSL::Crypto
        frame_work
    INCLUDE_DIRECTORIES  # Опционално
        ../../external_libs/nodeeditor/include
        ./Audio
        ./Library
    COMPILE_DEFINITIONS  # Опционално
        NODE_EDITOR_SHARED
        CUSTOM_FEATURE_ENABLED
    LINK_LIBRARIES      # Опционално
        additional_lib
    INSTALL_RPATH       # Опционално
        "$ORIGIN/../../lib"
)
```

**Характеристики:**
- Създава shared library
- Автоматично добавя `BUILD_AVAILABLE_PLUGIN` definition
- Стандартни include directories за plugins (`./`, `../../../frame_work/base/src/include`)
- **Допълнителни include directories** чрез `INCLUDE_DIRECTORIES` параметър
- **Допълнителни compile definitions** чрез `COMPILE_DEFINITIONS` параметър
- **Допълнителни библиотеки** чрез `LINK_LIBRARIES` параметър
- Install в `bin/` за runtime plugin discovery
- **Custom RPATH** чрез `INSTALL_RPATH` параметър (default: `$ORIGIN/../../lib`)
- **Всички допълнителни настройки се прилагат автоматично само ако dependencies са налични**

### 3. Вътрешни библиотеки - `create_internal_library()`
```cmake
create_internal_library(frame_work
    SOURCES
        base/src/QPluginManager.cpp
        base/src/QPluginInterface.cpp
        base/src/QPluginLoaderExt.cpp
        # ... други файлове
    REQUIRES_LIBRARIES
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Widgets
)
```

**Характеристики:**
- Създава shared library за вътрешна употреба
- Автоматично линква dependencies
- Install в `lib/`
- Export definitions чрез `FRAME_WORKSHARED_EXPORT`

### 4. Външни библиотеки - `create_external_library()`

Външните библиотеки се добавят чрез `create_external_library()`, което автоматично:
- Инициализира git submodule ако директорията липсва (при `DAQSTER_AUTO_INIT_SUBMODULES=ON`)
- Извиква `add_subdirectory()`
- Регистрира библиотеката като налична dependency
- Добавя copy wrapper target за копиране на артефакти в `build/lib/`

```cmake
# В root CMakeLists.txt — само за Qt5 (Qt6 compat issues)
if(QT_VERSION_MAJOR EQUAL 5)
    create_external_library(nodeeditor)   # target: nodes
    create_external_library(qtrest_lib)   # target: qtrest_lib
endif()
```

**Важно:** Параметърът на `create_external_library()` е името на директорията под `src/external_libs/`, но реалният CMake target може да е различен (например `nodeeditor` -> target `nodes`).

**Meta-target:**
```bash
cmake --build . --target daqster_build_externals  # билдва всички external libs
```

**CMake опции:**
```cmake
# Автоматично инициализиране на submodule ако директорията липсва (default: ON)
-DDAQSTER_AUTO_INIT_SUBMODULES=ON

# Показване на детайлни dependency проверки (default: OFF)
-DDAQSTER_VERBOSE_DEPENDENCIES=ON
```

## Управление на зависимости

### Автоматична проверка на зависимостите

Функцията `check_component_dependencies()` автоматично:

1. **За Qt модули** (`Qt5::Core`, `Qt6::Widgets`, etc.):
   - Извлича името на модула (напр. `Core` от `Qt5::Core`)
   - Динамично вика `find_package(Qt${QT_VERSION_MAJOR}${MODULE_NAME} QUIET)`
   - Проверява дали target-ът съществува
   
   ```cmake
   # Пример: Qt5::Charts
   string(REGEX REPLACE "^Qt[0-9]+::" "" MODULE_NAME ${LIBRARY})
   # MODULE_NAME = "Charts"
   find_package(Qt${QT_VERSION_MAJOR}Charts QUIET)
   if(TARGET Qt5::Charts)
       # Модулът е наличен
   endif()
   ```

2. **За external библиотеки** (`nodes`, `qtrest_lib`):
   - Проверява дали target-ът съществува
   - Проверява global property `EXTERNAL_LIB_${LIBRARY}_AVAILABLE`
   
   ```cmake
   if(TARGET nodes)
       # Библиотеката е налична
   elseif(EXTERNAL_LIB_nodes_AVAILABLE)
       # Библиотеката е регистрирана като налична
   endif()
   ```

3. **За packages** (`OpenSSL::SSL`, `OpenSSL::Crypto`):
   - Проверява дали target-ът съществува
   
   ```cmake
   if(TARGET OpenSSL::SSL)
       # Package-ът е наличен
   endif()
   ```

### Автоматично линкване

Функцията `link_component_dependencies()`:
- Линква всички библиотеки от `REQUIRES_LIBRARIES` списъка
- Автоматично предоставя include directories чрез target properties
- Не изисква ръчно добавяне на `target_link_libraries()`

```cmake
# Автоматично се извиква в create_plugin(), create_application(), etc.
link_component_dependencies(${COMPONENT_NAME})

# Под капака прави:
foreach(LIBRARY ${COMPONENT_LIBRARIES})
    if(TARGET ${LIBRARY})
        target_link_libraries(${COMPONENT_NAME} ${LIBRARY})
    endif()
endforeach()
```

### Условно изключване на компоненти

**Важно:** Dependency проверката се извършва **ПРЕДИ** `add_library()`/`add_executable()` командите. Ако dependencies липсват, template функцията излиза рано с `return()` и компонентът **изобщо не се създава**.

```cmake
# В create_plugin():
# 1. Първо се проверяват dependencies
register_component(${COMPONENT_NAME}
    REQUIRES_LIBRARIES ${PLUGIN_REQUIRES_LIBRARIES}
)

# 2. Проверка дали е enabled
get_property(COMPONENT_ENABLED GLOBAL PROPERTY COMPONENT_${COMPONENT_NAME}_ENABLED)

if(NOT COMPONENT_ENABLED)
    # Показва причините и излиза
    message(STATUS "Skipping plugin ${COMPONENT_NAME} - dependencies not met:")
    foreach(REASON ${REASONS})
        message(STATUS "  - ${REASON}")
    endforeach()
    return()  # НЕ се вика add_library()!
endif()

# 3. САМО ако dependencies са OK, се създава target
add_library(${COMPONENT_NAME} SHARED ${PLUGIN_SOURCES})
```

**Пример изход:**

```cmake
# Ако WebSockets липсва:
-- Checking Qt5::WebSockets - target exists: FALSE
-- QtCoinTraderPlugin plugin: DISABLED
--   - Qt5::WebSockets not available
-- Skipping plugin QtCoinTraderPlugin - dependencies not met:
--   - Qt5::WebSockets not available

# add_library(QtCoinTraderPlugin ...) НЕ се извиква
# target_include_directories(QtCoinTraderPlugin ...) НЕ се извиква
# Няма CMake грешки за missing targets
```

**Предимства:**
- Няма опити за компилиране на компоненти с липсващи dependencies
- Няма CMake грешки за missing targets
- Чист и ясен output за disabled компоненти
- Допълнителните настройки (INCLUDE_DIRECTORIES, COMPILE_DEFINITIONS) се прилагат само ако компонентът е enabled

## Qt Version Detection

### FindQtVersion.cmake

Модулът автоматично:
- Детектира Qt версия от `CMAKE_PREFIX_PATH` или `USE_QT6` опция
- Настройва `QT_VERSION_MAJOR`, `QT_PREFIX` променливи
- Намира core Qt модули (`Core`, `Gui`, `Widgets`)
- Позволява използването на `Qt${QT_VERSION_MAJOR}::Module` в dependencies

**Detection Logic:**
```cmake
# 1. Проверка на USE_QT6 flag
if(DEFINED USE_QT6)
    if(USE_QT6)
        set(QT_PREFER_QT6 TRUE)
    else()
        set(QT_PREFER_QT5 TRUE)
    endif()
# 2. Проверка на CMAKE_PREFIX_PATH
elseif(CMAKE_PREFIX_PATH MATCHES "qt/5\\.")
    set(QT_PREFER_QT5 TRUE)
elseif(CMAKE_PREFIX_PATH MATCHES "qt/6\\.")
    set(QT_PREFER_QT6 TRUE)
endif()

# 3. Find Qt version
if(QT_PREFER_QT5)
    find_package(Qt5 REQUIRED COMPONENTS Core Widgets Gui)
    set(QT_VERSION_MAJOR 5)
else()
    find_package(Qt6 REQUIRED COMPONENTS Core Widgets Gui)
    set(QT_VERSION_MAJOR 6)
endif()
```

**Usage Example:**
```cmake
# Автоматично работи и за Qt5 и за Qt6
REQUIRES_LIBRARIES
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Qml
```

## File Structure

```
Daqster/
├── CMakeLists.txt              # Root build file
├── cmake/
│   ├── FindQtVersion.cmake     # Qt version detection
│   ├── ComponentTemplates.cmake # Template functions (create_plugin, create_app, etc.)
│   └── PluginDependencyManager.cmake # Dependency checking functions
├── src/
│   ├── frame_work/
│   │   └── CMakeLists.txt      # Internal library (uses create_internal_library)
│   ├── external_libs/
│   │   ├── nodeeditor/
│   │   │   └── CMakeLists.txt  # External library (own build system)
│   │   └── qtrest_lib/
│   │       └── CMakeLists.txt  # External library (own build system)
│   ├── plugins/
│   │   ├── QtCoinTrader/
│   │   │   └── CMakeLists.txt  # Plugin (uses create_plugin)
│   │   ├── node_editor/
│   │   │   └── CMakeLists.txt  # Plugin (uses create_plugin)
│   │   └── tests/
│   │       ├── plugin_main_test/
│   │       │   └── CMakeLists.txt # Test plugin (uses create_plugin)
│   │       ├── plugin_fancy_test/
│   │       ├── plugin_uggly_test/
│   │       └── template_plugin_daqster/
│   └── apps/
│       └── Daqster/
│           └── CMakeLists.txt  # Application (uses create_application)
```

## Build Order

```cmake
# Root CMakeLists.txt

# 1. Include build system modules
include(FindQtVersion)
include(PluginDependencyManager)
include(ComponentTemplates)

# 2. Core framework (no external dependencies)
add_subdirectory(src/frame_work)

# 3. External libraries — само Qt5 (Qt6 compat issues)
if(QT_VERSION_MAJOR EQUAL 5)
    create_external_library(nodeeditor)   # -> target: nodes
    create_external_library(qtrest_lib)   # -> target: qtrest_lib
else()
    message(STATUS "External libraries disabled for Qt6 (compatibility issues)")
endif()

# 4. Plugins — всички се добавят; dependency системата ги изключва автоматично
add_subdirectory(src/plugins/node_editor)        # Qt5-only (изисква nodes)
add_subdirectory(src/plugins/QtCoinTrader)       # Qt5-only (изисква qtrest_lib)
add_subdirectory(src/plugins/tests/plugin_main_test)
add_subdirectory(src/plugins/tests/plugin_fancy_test)
add_subdirectory(src/plugins/tests/plugin_uggly_test)
add_subdirectory(src/plugins/tests/template_plugin_daqster)

# 5. Applications
add_subdirectory(src/apps/Daqster)

# 6. Build summary
print_build_configuration_summary()
```

## Conditional Building

### Според версията на Qt

Всички plugin `add_subdirectory()` се извикват винаги. Dependency системата автоматично ги изключва ако нужните external libs липсват:

```
Qt5 build:  NodeEditorPlugin включен, QtCoinTraderPlugin включен, test plugins включени
Qt6 build:  NodeEditorPlugin изключен, QtCoinTraderPlugin изключен, test plugins включени
```

> Qt6 ограничение: `NodeEditorPlugin` и `QtCoinTraderPlugin` не се компилират при Qt6, защото външните библиотеки `nodeeditor` и `qtrest_lib` имат проблеми със съвместимостта с Qt6 и са изключени. Само test плъгините (`plugin_main_test`, `plugin_fancy_test`, `plugin_uggly_test`, `plugin_template_test`) работят с Qt6.

### Според зависимости (автоматично)
Компонентът се изключва автоматично ако dependencies липсват:

```cmake
# Plugin с OpenSSL dependency
create_plugin(QtCoinTraderPlugin
    SOURCES ...
    REQUIRES_LIBRARIES
        OpenSSL::SSL      # Автоматично проверява
        OpenSSL::Crypto   # Автоматично проверява
        qtrest_lib        # Автоматично проверява
        frame_work
)

# Ако OpenSSL липсва:
# -- Checking OpenSSL::SSL - target exists: FALSE
# -- QtCoinTraderPlugin plugin: DISABLED
# --   - OpenSSL::SSL not available
```

## Откриване на plugins по време на изпълнение

### Пътища за търсене на plugins

`QPluginManager` търси plugins в следните директории (по приоритет):

1. **Build directory** (highest priority for development):
   - `{applicationDirPath}`
   - `{applicationDirPath}/plugins`
   
2. **Install directory**:
   - `{applicationDirPath}/../lib/daqster/plugins`
   
3. **Environment variables**:
   - `DAQSTER_PLUGIN_DIR`
   - `DAQSTER_PLUGIN_PATH` (multiple paths separated by `:`)
   
4. **User directory**:
   - `~/.local/share/daqster/plugins`
   
5. **System directories** (lowest priority):
   - `/usr/lib/daqster/plugins`
   - `/usr/local/lib/daqster/plugins`

### Configuration

Plugin search paths се конфигурират в `QPluginManager.cpp`:

```cpp
// Build директория (най-висок приоритет за дебъг)
m_DirList.append(qApp->applicationDirPath());  // Търси в bin/ директорията
m_DirList.append(qApp->applicationDirPath()+QString("/plugins"));
m_DirList.append(QDir(qApp->applicationDirPath()+"/../lib/daqster/plugins").absolutePath());

// Environment variable override
const QString envDir = qgetenv("DAQSTER_PLUGIN_DIR");
if (!envDir.isEmpty()) {
    m_DirList.append(envDir);
}

// User plugins directory
QString userPluginDir = QDir::homePath() + "/.local/share/daqster/plugins";
m_DirList.append(userPluginDir);
```

## Примери

### Добавяне на нов plugin

1. Създайте директория: `src/plugins/MyNewPlugin/`

2. Създайте `CMakeLists.txt`:
```cmake
create_plugin(MyNewPlugin
    SOURCES
        MyPluginInterface.cpp
        MyPluginObject.cpp
        MyPluginInterface.h
        MyPluginObject.h
        resources.qrc
    REQUIRES_LIBRARIES
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        frame_work
)
```

3. Добавете в root `CMakeLists.txt`:
```cmake
# Plugins section
add_subdirectory(src/plugins/MyNewPlugin)
```

4. Build:
```bash
cmake --build build --target MyNewPlugin
```

### Добавяне на plugin с външни зависимости и допълнителни настройки

```cmake
create_plugin(AdvancedPlugin
    SOURCES
        AdvancedInterface.cpp
        AdvancedObject.cpp
        utils/Helper.cpp
    REQUIRES_LIBRARIES
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        Qt${QT_VERSION_MAJOR}::Network
        Qt${QT_VERSION_MAJOR}::Charts
        nodes              # External library
        OpenSSL::SSL       # System package
        frame_work
    INCLUDE_DIRECTORIES
        ../../external_libs/nodes/include
        ./utils
        ./models
    COMPILE_DEFINITIONS
        ADVANCED_PLUGIN_VERSION="1.0.0"
        ENABLE_ADVANCED_FEATURES
    LINK_LIBRARIES
        custom_helper_lib
    INSTALL_RPATH
        "$ORIGIN/../../lib:$ORIGIN/../custom"
)

# Всички допълнителни настройки (INCLUDE_DIRECTORIES, COMPILE_DEFINITIONS, etc.)
# се прилагат автоматично САМО ако dependencies са налични.
# Ако nodes или OpenSSL липсват, plugin-ът се изключва и никакви target
# команди не се извикват (няма CMake грешки).
```

### Добавяне на външна библиотека

1. Поставете библиотеката в `src/external_libs/mylibrary/`

2. В root `CMakeLists.txt`:
```cmake
# External libraries section
add_subdirectory(src/external_libs/mylibrary)
register_external_library_dependency(mylibrary)
```

3. Използвайте в plugin:
```cmake
create_plugin(MyPlugin
    SOURCES ...
    REQUIRES_LIBRARIES
        mylibrary  # Автоматично се проверява
        frame_work
)
```

### Добавяне на ново приложение

1. Създайте директория: `src/apps/MyApp/`

2. Създайте `CMakeLists.txt`:
```cmake
create_application(MyApp
    SOURCES
        main.cpp
        MainWindow.cpp
        MainWindow.h
        mainwindow.ui
        resources.qrc
    REQUIRES_LIBRARIES
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Widgets
        frame_work
)

# Additional include directories if needed
target_include_directories(MyApp PRIVATE ./)
```

3. Добавете в root `CMakeLists.txt`:
```cmake
# Apps section
add_subdirectory(src/apps/MyApp)
```

## Справка за CMake функциите

### Шаблони за компоненти (ComponentTemplates.cmake)

#### `create_application(COMPONENT_NAME SOURCES ... REQUIRES_LIBRARIES ...)`
Създава executable application.

**Parameters:**
- `COMPONENT_NAME` - Име на application-а
- `SOURCES` - Списък с source файлове
- `REQUIRES_LIBRARIES` - Списък с dependencies

**Example:**
```cmake
create_application(Daqster
    SOURCES main.cpp MainWindow.cpp
    REQUIRES_LIBRARIES Qt5::Core Qt5::Widgets frame_work
)
```

#### `create_plugin(COMPONENT_NAME SOURCES ... REQUIRES_LIBRARIES ...)`
Създава plugin shared library.

**Parameters:**
- `COMPONENT_NAME` - Име на plugin-а
- `SOURCES` - Списък с source файлове  
- `REQUIRES_LIBRARIES` - Списък с dependencies

**Automatic Features:**
- Добавя `BUILD_AVAILABLE_PLUGIN` definition
- Настройва стандартни include directories
- Конфигурира RPATH за runtime linking

**Example:**
```cmake
create_plugin(NodeEditorPlugin
    SOURCES NodeEditorInterface.cpp NodeEditorObject.cpp
    REQUIRES_LIBRARIES Qt5::Core Qt5::Gui nodes frame_work
)
```

#### `create_internal_library(COMPONENT_NAME SOURCES ... REQUIRES_LIBRARIES ...)`
Създава internal shared library.

**Parameters:**
- `COMPONENT_NAME` - Име на библиотеката
- `SOURCES` - Списък с source файлове
- `REQUIRES_LIBRARIES` - Списък с dependencies

**Example:**
```cmake
create_internal_library(frame_work
    SOURCES QPluginManager.cpp QPluginInterface.cpp
    REQUIRES_LIBRARIES Qt5::Core Qt5::Widgets
)
```

### Управление на зависимости (PluginDependencyManager.cmake)

#### `register_component(COMPONENT_NAME REQUIRES_LIBRARIES ...)`
Регистрира компонент и проверява dependencies.

**Internal Function** - извиква се автоматично от template functions.

#### `check_plugin_dependencies(COMPONENT_NAME REQUIRES_LIBRARIES ...)`
Проверява дали всички dependencies са налични.

**Returns:**
- `${COMPONENT_NAME}_ENABLED` - TRUE/FALSE
- `${COMPONENT_NAME}_REASONS` - Списък с причини ако е DISABLED

#### `link_component_dependencies(COMPONENT_NAME)`
Автоматично линква всички dependencies на компонент.

**Internal Function** - извиква се автоматично от template functions.

#### `create_external_library(COMPONENT_NAME)`
Добавя external библиотека (submodule), регистрира я и настройва copy target.

**Example:**
```cmake
create_external_library(nodeeditor)  # добавя src/external_libs/nodeeditor
create_external_library(qtrest_lib)  # добавя src/external_libs/qtrest_lib
```

#### `register_external_library_dependency(LIBRARY_NAME)`
Ниско-ниво функция — регистрира target като налична external dependency. Извиква се автоматично от `create_external_library()`.

#### `verbose_status(...)`
Извежда STATUS съобщение само ако `DAQSTER_VERBOSE_DEPENDENCIES=ON`.

#### `print_component_status_summary()`
Показва статус на всички регистрирани компоненти.

#### `print_build_configuration_summary()`
Показва обща информация за build конфигурацията.

## Променливи на build системата

### CMake Options (команден ред)

| Опция | Default | Описание |
|---|---|---|
| `USE_QT6` | — | `ON` за Qt6, `OFF` за Qt5; ако не е зададен — auto-detect от `CMAKE_PREFIX_PATH` |
| `DAQSTER_VERBOSE_DEPENDENCIES` | `OFF` | Показва детайлни dependency проверки при configure |
| `DAQSTER_AUTO_INIT_SUBMODULES` | `ON` | Автоматично `git submodule update --init` при липсваща external директория |

**Пример:**
```bash
cmake -S . -B build \
  -DUSE_QT6=OFF \
  -DDAQSTER_VERBOSE_DEPENDENCIES=ON \
  -DCMAKE_PREFIX_PATH=/path/to/Qt/5.15.2/gcc_64
```

### Global Variables

- `QT_VERSION_MAJOR` - Основна версия на Qt (5 или 6)
- `QT_PREFIX` - Prefix за Qt targets (`Qt5` или `Qt6`)
- `QT_VERSION` - Пълна версия на Qt (напр. `5.15.2`)

### Component Properties

За всеки компонент се създават следните global properties:

- `COMPONENT_${NAME}_ENABLED` - Дали компонентът е enabled
- `COMPONENT_${NAME}_REASONS` - Причини за disable (ако има)
- `COMPONENT_${NAME}_LIBRARIES` - Списък с dependencies

### External Library Properties

- `EXTERNAL_LIB_${NAME}_AVAILABLE` - Дали external библиотеката е налична

## Предимства

1. **Опростена структура**
   - Всеки компонент е self-contained
   - Минимален boilerplate код
   - Ясна и консистентна структура

2. **Автоматично dependency management**
   - Не е нужно ръчно линкване
   - Автоматична проверка на наличност
   - Условно изключване на компоненти

3. **Qt5/Qt6 портабилност**
   - Единна синтаксис за двете версии
   - Автоматично детектиране на версия
   - Dynamic module discovery

4. **Лесно разширяване**
   - Добавяне на нов plugin - 1 файл
   - Добавяне на external библиотека - 2 реда
   - Ясни template functions

5. **Build-time optimization**
   - Само нужните модули се търсят
   - Conditional compilation
   - Бързо CMake configuration

6. **Ясна архитектура**
   - Template-based approach
   - Separation of concerns
   - Self-documenting code

## Отстраняване на проблеми

### Plugin не се намира при runtime

**Симптом:**
```
PluginsList count: 0
```

**Причина:** Plugin search paths не включват build директорията

**Решение:** Проверете `QPluginManager.cpp`:
```cpp
m_DirList.append(qApp->applicationDirPath());  // Трябва да е първи
```

### Dependency не се намира при build

**Симптом:**
```
-- Checking Qt5::Charts - target exists: FALSE
-- QtCoinTraderPlugin plugin: DISABLED
--   - Qt5::Charts not available
```

**Причина:** Qt модулът не е инсталиран или не се намира

**Решение:**
1. Инсталирайте липсващия Qt модул
2. Или премахнете dependency от `REQUIRES_LIBRARIES`
3. Или направете dependency условно:
```cmake
if(Qt${QT_VERSION_MAJOR}Charts_FOUND)
    target_link_libraries(MyPlugin Qt${QT_VERSION_MAJOR}::Charts)
    target_compile_definitions(MyPlugin PRIVATE CHARTS_AVAILABLE)
endif()
```

### External библиотека не се намира

**Симптом:**
```
-- Checking nodes - target exists: FALSE
-- NodeEditorPlugin plugin: DISABLED
--   - nodes not available
```

**Причина:** External библиотеката не е регистрирана

**Решение:** Добавете след `add_subdirectory()`:
```cmake
add_subdirectory(src/external_libs/nodeeditor)
register_external_library_dependency(nodes)  # Задължително!
```

### Qt version detection грешка

**Симптом:**
```
CMake Error: Could not find Qt5 or Qt6
```

**Причина:** `CMAKE_PREFIX_PATH` не сочи към Qt

**Решение:**
```bash
# Задайте CMAKE_PREFIX_PATH
cmake -DCMAKE_PREFIX_PATH=/path/to/Qt/5.15.2/gcc_64 ..

# Или използвайте USE_QT6 flag
cmake -DUSE_QT6=OFF ..  # За Qt5
cmake -DUSE_QT6=ON ..   # За Qt6
```

### MOC errors при build

**Симптом:**
```
fatal error: QObject: No such file or directory
```

**Причина:** Липсващи include directories

**Решение:** Template functions автоматично добавят include directories, но ако имате custom includes:
```cmake
create_plugin(MyPlugin
    SOURCES ...
    REQUIRES_LIBRARIES Qt${QT_VERSION_MAJOR}::Core
)

# Допълнителни includes
target_include_directories(MyPlugin PRIVATE 
    ./my_include_dir
    ../../some_other_dir
)
```

### RPATH errors при runtime

**Симптом:**
```
error while loading shared libraries: libframe_work.so: cannot open shared object file
```

**Причина:** RPATH не е настроен правилно

**Решение:** Template functions автоматично настройват RPATH, но проверете:
```cmake
# За plugins
set_target_properties(${COMPONENT_NAME} PROPERTIES
    INSTALL_RPATH "$ORIGIN/../../lib"
)

# За applications
set_target_properties(${COMPONENT_NAME} PROPERTIES
    INSTALL_RPATH "$ORIGIN/../lib"
)
```

## Добри практики

1. **Използвайте `Qt${QT_VERSION_MAJOR}::` за всички Qt dependencies**
   ```cmake
   # Добре
   REQUIRES_LIBRARIES Qt${QT_VERSION_MAJOR}::Core
   
   # Лошо
   REQUIRES_LIBRARIES Qt5::Core  # Hardcoded версия
   ```

2. **Декларирайте всички dependencies изрично**
   ```cmake
   # Добре - всички dependencies са видими
   REQUIRES_LIBRARIES
       Qt${QT_VERSION_MAJOR}::Core
       Qt${QT_VERSION_MAJOR}::Gui
       Qt${QT_VERSION_MAJOR}::Network
       frame_work
   
   # Лошо - скрити dependencies
   REQUIRES_LIBRARIES frame_work
   # И после ръчно:
   target_link_libraries(MyPlugin Qt5::Network)
   ```

3. **Регистрирайте external библиотеки след add_subdirectory**
   ```cmake
   # Добре
   add_subdirectory(src/external_libs/mylibrary)
   register_external_library_dependency(mylibrary)
   
   # Лошо
   register_external_library_dependency(mylibrary)
   add_subdirectory(src/external_libs/mylibrary)
   ```

4. **Използвайте template functions, не ръчен CMake код**
   ```cmake
   # Добре
   create_plugin(MyPlugin
       SOURCES ...
       REQUIRES_LIBRARIES ...
   )
   
   # Лошо
   add_library(MyPlugin SHARED ...)
   target_link_libraries(MyPlugin ...)
   install(TARGETS MyPlugin ...)
   # ... много ръчен код
   ```

5. **Групирайте свързани компоненти**
   ```cmake
   # Root CMakeLists.txt - ясна структура
   
   # Framework
   add_subdirectory(src/frame_work)
   
   # External libraries
   if(QT_VERSION_MAJOR EQUAL 5)
       add_subdirectory(src/external_libs/nodeeditor)
       add_subdirectory(src/external_libs/qtrest_lib)
   endif()
   
   # Plugins
   add_subdirectory(src/plugins/node_editor)
   add_subdirectory(src/plugins/QtCoinTrader)
   
   # Applications
   add_subdirectory(src/apps/Daqster)
   ```

## Ръководство за миграция

### От старата система към новата

#### Преди (PluginDependencyManager стил):
```cmake
# src/plugins/CMakeLists.txt
register_plugin(MyPlugin
    REQUIRES_QT_MODULES Core Gui Network
    REQUIRES_EXTERNAL_LIBS mylibrary
)
add_plugin_subdirectory(MyPlugin my_plugin)

# src/plugins/my_plugin/CMakeLists.txt
add_library(MyPlugin SHARED ...)
link_plugin_dependencies(MyPlugin)
target_link_libraries(MyPlugin Qt5::Network)
install(TARGETS MyPlugin ...)
```

#### След (шаблонен стил):
```cmake
# src/plugins/my_plugin/CMakeLists.txt
create_plugin(MyPlugin
    SOURCES
        MyInterface.cpp
        MyObject.cpp
    REQUIRES_LIBRARIES
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        Qt${QT_VERSION_MAJOR}::Network
        mylibrary
        frame_work
)

# Root CMakeLists.txt
add_subdirectory(src/plugins/my_plugin)
```

**Ключови промени:**
1. Премахнат централизиран `src/plugins/CMakeLists.txt`
2. Всеки plugin е self-contained в собствената си директория
3. Използва се `create_plugin()` вместо ръчен CMake код
4. Dependencies са в един `REQUIRES_LIBRARIES` списък
5. Автоматично линкване, не е нужно `link_plugin_dependencies()`

## Обобщение на добрите практики

### Използвайте новите параметри на create_plugin()

**Добре:** Всичко е декларирано в `create_plugin()` извикването
```cmake
create_plugin(NodeEditorPlugin
    SOURCES ...
    REQUIRES_LIBRARIES ...
    INCLUDE_DIRECTORIES
        ../../external_libs/nodeeditor/include
        ./Audio
    COMPILE_DEFINITIONS
        NODE_EDITOR_SHARED
)
```

**Лошо:** Ръчни проверки и target команди извън template функцията
```cmake
create_plugin(NodeEditorPlugin
    SOURCES ...
    REQUIRES_LIBRARIES ...
)

# Лошо - ръчна проверка и target команди
get_property(COMPONENT_ENABLED GLOBAL PROPERTY COMPONENT_NodeEditorPlugin_ENABLED)
if(COMPONENT_ENABLED)
    target_include_directories(NodeEditorPlugin ...)
    target_compile_definitions(NodeEditorPlugin ...)
endif()
```

### Предимства на новия подход

1. **Чист и декларативен синтаксис** - всичко е на едно място
2. **Автоматично управление** - template функцията се грижи за всичко
3. **Няма грешки за missing targets** - допълнителните настройки се прилагат само ако компонентът е enabled
4. **По-лесна поддръжка** - ясно и кратко

### Поток на проверка на зависимостите

```
create_plugin(MyPlugin ...)
    ↓
register_component(MyPlugin) - проверява dependencies
    ↓
get_property(COMPONENT_ENABLED)
    ↓
if (NOT ENABLED) -> return()  <- НЕ се вика add_library()!
    ↓
if (ENABLED) -> add_library(MyPlugin ...)
             -> target_include_directories() (ако INCLUDE_DIRECTORIES е зададен)
             -> target_compile_definitions() (ако COMPILE_DEFINITIONS е зададен)
             -> target_link_libraries() (автоматично + LINK_LIBRARIES)
             -> install()
             -> set_target_properties()
```

## История на версиите

### Версия 2.1 (текуща - October 2025)
- **Елегантни template параметри**: `INCLUDE_DIRECTORIES`, `COMPILE_DEFINITIONS`, `LINK_LIBRARIES`, `INSTALL_RPATH`
- **Early return при липсващи dependencies**: Няма опити за създаване на targets с неизпълнени dependencies
- **Автоматично прилагане на настройки**: Допълнителните target команди се извикват само ако компонентът е enabled
- **Премахнати ръчни проверки**: Няма нужда от `get_property(COMPONENT_ENABLED)` в plugin CMakeLists.txt
- **Improved error messages**: Ясни съобщения за disabled компоненти с причини

### Версия 2.0 (2024)
- Template-based component creation
- Self-contained components
- Dynamic Qt module discovery
- Simplified architecture
- Removed centralized plugin registration

### Версия 1.0 (стара система)
- Centralized plugin registration
- Separate dependency types (QT_MODULES, EXTERNAL_LIBS, PACKAGES)
- Manual linking with `link_plugin_dependencies()`
- Pre-discovery of all Qt modules

## Вижте също

- `cmake/ComponentTemplates.cmake` - Template function implementations
- `cmake/PluginDependencyManager.cmake` - Dependency checking logic
- `cmake/FindQtVersion.cmake` - Qt version detection
- `src/frame_work/base/src/include/plugin_global.h` - Plugin export macros

