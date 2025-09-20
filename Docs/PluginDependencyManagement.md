# Plugin Dependency Management System

## Общ преглед

Опростената Plugin Dependency Management система позволява автоматично управление на plugin dependencies и условно компилиране на plugins базирано на наличните Qt модули, external библиотеки и packages.

## Основни функции

### 1. Plugin Registration
```cmake
register_plugin(PluginName
    REQUIRES_QT_MODULES Core Widgets Qml
    REQUIRES_EXTERNAL_LIBS qtrest_lib some_lib
    REQUIRES_PACKAGES OpenSSL
)
```

### 2. External Library Registration
```cmake
register_external_library(library_name)
```

### 3. Conditional Plugin Building
```cmake
add_plugin_subdirectory(PluginName src/plugins/plugin_dir)
```

### 4. Dependency Checking
- **Qt Modules**: Автоматично проверява наличността на Qt модули
- **External Libraries**: Проверява дали target-ите съществуват
- **Packages**: Проверява дали packages са намерени от find_package()

## Структура на системата

### PluginDependencyManager.cmake
Опростеният модул съдържа само нужните функции:

- `register_plugin()` - Регистрира plugin с dependencies
- `register_external_library()` - Регистрира external библиотека
- `check_plugin_dependencies()` - Проверява plugin dependencies
- `check_external_library_buildability()` - Проверява external библиотеки
- `add_plugin_subdirectory()` - Добавя plugin директория условно
- `is_plugin_enabled()` - Helper функция за проверка на статус
- `print_plugin_status_summary()` - Показва статус на всички plugins
- `print_build_configuration_summary()` - Показва build конфигурация

### PluginExamples.cmake
Съдържа примери за различни типове plugin dependencies:

- Прости plugins (само Qt модули)
- Plugins с external библиотеки
- Plugins с package dependencies
- Сложни plugins с множество dependencies
- Qt6-специфични plugins
- Debug-only plugins
- Plugins с custom dependency checks

## Използване

### 1. В главния CMakeLists.txt
```cmake
# Include dependency manager
include(PluginDependencyManager)

# Register external libraries - те проверяват собствените си dependencies
register_external_library(nodeeditor)
register_external_library(qtrest_lib)
```

### 2. В plugins/CMakeLists.txt
```cmake
# Register plugins
register_plugin(QtCoinTraderPlugin
    REQUIRES_QT_MODULES Qml Quick QuickControls2 Charts
    REQUIRES_EXTERNAL_LIBS qtrest_lib
    REQUIRES_PACKAGES OpenSSL
)

# Add plugin subdirectories
add_plugin_subdirectory(QtCoinTraderPlugin QtCoinTrader)
```

## Типове Dependencies

### Qt Modules
```cmake
REQUIRES_QT_MODULES Core Widgets Qml Quick QuickControls2 Charts Network
```
- Автоматично проверява наличността на Qt модули
- Използва QT_*_LIB променливи

### External Libraries
```cmake
REQUIRES_EXTERNAL_LIBS qtrest_lib nodes some_lib
```
- Проверява дали CMake target-ите съществуват
- Използва `TARGET` проверка

### Packages
```cmake
REQUIRES_PACKAGES OpenSSL
```
- Проверява дали packages са намерени
- Използва `*_FOUND` променливи

## Debug и Logging

Системата предоставя подробна информация за:

- Статус на всеки plugin (ENABLED/DISABLED)
- Причини за изключване на plugins
- Build конфигурация
- External library buildability

### Примерен изход:
```
=== Plugin Status Summary ===
✓ NodeEditorLibrary: ENABLED
✓ QtRestLibrary: ENABLED
✓ NodeEditorPlugin: ENABLED
✓ QtCoinTraderPlugin: ENABLED
✓ TestPlugins: ENABLED

=== Build Configuration Summary ===
Qt Version: 5.15.2
Build Type: Debug
C++ Standard: 14
Compiler: GCC 9.3.0

External Libraries Buildability:
  - nodeeditor: TRUE
  - qtrest_lib: TRUE
```

## Опростена архитектура

### Принципи:
1. **External библиотеки** сами си проверяват dependencies-ите в техните CMake файлове
2. **Plugins** декларират dependencies-ите си в `register_plugin()`
3. **Няма дублиране** - единствен източник на истината за dependencies
4. **Самостоятелност** - всеки plugin може да се build-ва независимо

### Предимства на опростената система:

1. **Автоматично управление**: Не е нужно ръчно да проверяваме dependencies
2. **Гъвкавост**: Лесно добавяне на нови plugins с различни dependencies
3. **Debug информация**: Подробна информация защо plugins са изключени
4. **Консистентност**: Еднаква логика за всички plugins
5. **Разширяемост**: Лесно добавяне на нови типове dependencies
6. **Простота**: По-малко custom функции, по-лесно за разбиране
7. **Самостоятелност**: External библиотеките са независими
8. **Няма дублиране**: Dependencies се декларират само веднъж

## Миграция от стария подход

### Преди (сложен подход):
```cmake
# В главния CMakeLists.txt
if(QT_VERSION_MAJOR EQUAL 5)
    add_subdirectory(src/external_libs/nodeeditor)
    add_subdirectory(src/external_libs/qtrest_lib)
endif()

# В plugins/CMakeLists.txt
if(QT_QUICKCONTROLS2_LIB AND NOT "${QT_QUICKCONTROLS2_LIB}" STREQUAL "" AND TARGET qtrest_lib)
    add_subdirectory(QtCoinTrader)
    message(STATUS "QtCoinTrader plugin enabled")
else()
    message(STATUS "QtCoinTrader plugin disabled")
endif()
```

### След (опростен подход):
```cmake
# В главния CMakeLists.txt
register_external_library(nodeeditor)
register_external_library(qtrest_lib)

# В plugins/CMakeLists.txt
register_plugin(QtCoinTraderPlugin
    REQUIRES_QT_MODULES Qml Quick QuickControls2
    REQUIRES_EXTERNAL_LIBS qtrest_lib
)

add_plugin_subdirectory(QtCoinTraderPlugin QtCoinTrader)
```

## Бъдещи подобрения

1. **Custom dependency checks**: Възможност за custom логика за проверка
2. **Version requirements**: Поддръжка за минимални версии на dependencies
3. **Optional dependencies**: Dependencies които не са задължителни
4. **Plugin groups**: Групиране на plugins по функционалност
5. **Build optimization**: Автоматично изключване на ненужни dependencies
