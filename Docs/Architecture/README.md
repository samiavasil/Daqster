# Архитектура на Daqster

[Български](./README.md) | [English](./README.en.md)

Родител: [Documentation Index](../index.md)

## Архитектурен хъб

- [BuildSystemArchitecture.md](./BuildSystemArchitecture.md) - build архитектура и зависимости
- [apps/README.md](./apps/README.md) - application подсистема
- [framework/README.md](./framework/README.md) - framework подсистема
- [plugins/README.md](./plugins/README.md) - plugin подсистема

## Свързани секции

- [Development Topics](../development/README.md) - workflow за разработка и дебъг материали

## Общ преглед

Daqster е Qt5-базирана рамка за създаване и зареждане на плъгини с хост приложение. Проектът използва модулна архитектура, която позволява лесно разширяване чрез динамично зареждане на плъгини.

## Структура на проекта

```
Daqster/
├── src/                          # Source код
│   ├── frame_work/               # Ядро на рамката
│   │   └── base/                 # Основни класове
│   │       ├── src/              # Implementation
│   │       └── include/          # Headers
│   ├── apps/                     # Приложения
│   │   └── Daqster/              # Главно приложение
│   ├── plugins/                  # Плъгини
│   │   ├── NodeEditor/           # Node Editor плъгин
│   │   ├── QtCoinTrader/         # QtCoinTrader плъгин
│   │   └── tests/                # Тестови плъгини
│   └── external_libs/            # Външни библиотеки
│       ├── nodeeditor/           # Node Editor библиотека
│       └── qtrest_lib/           # REST API библиотека
├── tools/                        # Инструменти за билд
│   ├── create_appimage.sh        # AppImage създаване
│   └── Build_AppImage/           # Локални AppImage билдове
├── Docs/                         # Документация
│   ├── Architecture/             # Архитектурна документация
│   │   ├── README.md             # Тази документация
│   │   ├── README.en.md          # English версия
│   │   ├── BuildSystemArchitecture.md
│   │   ├── apps/
│   │   ├── framework/
│   │   └── plugins/
│   ├── development/              # Разработка и дебъг
│   │   ├── README.md
│   │   ├── DeveloperGuide.md
│   │   └── HowToDebugAppImage.md
│   ├── operations/               # Оперативни и build теми
│   │   ├── BuildHelpers.md
│   │   └── UpstreamManagement.md
│   └── porting/                  # Портинг и миграции
│       └── QtRest_Qt6_Porting.md
├── .github/workflows/            # CI/CD
│   ├── ci.yml                    # Continuous Integration
│   └── release.yml               # Release workflow
└── CMakeLists.txt                # Главен CMake файл
```

## Основни компоненти

### 1. Frame Work (Ядро)

**Местоположение:** `src/frame_work/base/`

**Основни класове:**
- `QPluginManager` - Управление на плъгини
- `QPluginInterface` - Базов интерфейс за плъгини
- `QPluginLoaderExt` - Разширено зареждане на плъгини
- `PluginFilter` - Филтриране на плъгини

**Отговорности:**
- Откриване на плъгини в различни директории
- Зареждане и инициализиране на плъгини
- Управление на жизнения цикъл на плъгини
- Филтриране по тип и свойства

### 2. Host Application (Главно приложение)

**Местоположение:** `src/apps/Daqster/`

**Основни класове:**
- `main.cpp` - Точка на влизане
- `ApplicationsManager` - Управление на child приложения
- `AppToolbar` - GUI toolbar за стартиране на плъгини

**Отговорности:**
- Инициализиране на Qt приложението
- Зареждане на плъгини при стартиране
- Стартиране на плъгини като отделни процеси
- Управление на GUI елементи

### 3. Plugin System (Система за плъгини)

**Местоположение:** `src/plugins/`

**Типове плъгини:**
- **APPLICATION_PLUGIN** - Самостоятелни приложения
- **DETECT_BY_TYPE_NAME** - Плъгини с custom тип

**Plugin Discovery:**
1. Build директория (`./plugins`, `../lib/daqster/plugins`)
2. Environment variables (`DAQSTER_PLUGIN_DIR`, `DAQSTER_PLUGIN_PATH`)
3. User plugins (`~/.local/share/daqster/plugins`)
4. System plugins (`/usr/lib/daqster/plugins`)

### 4. External Libraries (Външни библиотеки)

**Местоположение:** `src/external_libs/`

**Библиотеки:**
- **nodeeditor** - Графичен редактор за нодове
- **qtrest_lib** - REST API клиент

## Архитектурни принципи

### 1. Модулност
- Всеки компонент е независим модул
- Ясно разделение на отговорности
- Лесно тестване и поддръжка

### 2. Разширяемост
- Динамично зареждане на плъгини
- Plugin discovery система
- Поддръжка за различни типове плъгини

### 3. Изолиране
- Плъгини се стартират като отделни процеси
- Environment variables за изолация
- Независими конфигурационни файлове

### 4. Крос-платформеност
- Qt5 за GUI и крос-платформеност
- CMake за build система
- AppImage за Linux разпространение

## Data Flow

### 1. Стартиране на приложението
```
main.cpp
├── Инициализира QApplication
├── Създава QPluginManager
├── Зарежда плъгини
└── Стартира GUI
```

### 2. Зареждане на плъгини
```
QPluginManager
├── Сканира директории за плъгини
├── Зарежда .so файлове
├── Инициализира QPluginInterface
└── Добавя в списък с активни плъгини
```

### 3. Стартиране на плъгин
```
AppToolbar/ApplicationsManager
├── Получава заявка за стартиране
├── Намира плъгин по име
├── Създава QProcess с environment
└── Стартира като child процес
```

## Архитектурна диаграма

![Architecture Diagram](../diagrams/architecture.svg)

[PlantUML източник](../diagrams/architecture.puml)

## Plugin Discovery Flow

![Plugin Discovery Flow](../diagrams/startup_sequence.svg)

[PlantUML източник](../diagrams/startup_sequence.puml)

## Environment Variables

### Plugin Discovery
- `DAQSTER_PLUGIN_DIR` - Една директория за плъгини
- `DAQSTER_PLUGIN_PATH` - Множество директории (разделени с `:`)

### Qt Environment
- `LD_LIBRARY_PATH` - Пътища към споделени библиотеки
- `QML2_IMPORT_PATH` - Пътища към QML модули
- `QT_PLUGIN_PATH` - Пътища към Qt плъгини
- `QT_QPA_PLATFORM_PLUGIN_PATH` - Пътища към platform плъгини

### XDG Directories
- `XDG_CONFIG_HOME` - Конфигурационни файлове
- `XDG_DATA_HOME` - Данни
- `XDG_CACHE_HOME` - Кеш

## Security Considerations

### 1. Plugin Isolation
- Плъгини се стартират като отделни процеси
- Ограничен достъп до системни ресурси
- Environment variables за изолация

### 2. Plugin Validation
- Проверка на plugin metadata
- Валидация на plugin интерфейси
- Hash-based дедупликация

### 3. File System Access
- Ограничен достъп до файловата система
- XDG directories за user data
- Read-only AppImage среда

## Performance Considerations

### 1. Plugin Loading
- Lazy loading на плъгини
- Кеширане на plugin metadata
- Hash-based дедупликация

### 2. Memory Management
- RAII за автоматично управление на паметта
- Smart pointers за shared ресурси
- Правилно освобождаване на Qt обекти

### 3. Process Management
- Ефективно стартиране на child процеси
- Правилно управление на environment variables
- Graceful shutdown на процеси

## Testing Strategy

### 1. Unit Tests
- Тестване на отделни компоненти
- Mock обекти за изолация
- Automated test execution

### 2. Integration Tests
- Тестване на plugin зареждане
- Тестване на plugin стартиране
- End-to-end тестове

### 3. AppImage Tests
- Тестване на AppImage функционалност
- Тестване на environment variables
- Cross-platform compatibility

## 9. Plugin Lifecycle Diagram

![Plugin Lifecycle](../diagrams/plugin_lifecycle.svg)

[PlantUML източник](../diagrams/plugin_lifecycle.puml)

## 10. AppImage Structure

```
Daqster-x86_64.AppImage
├── AppRun                          ← startup script
├── daqster.desktop
├── daqster.png
└── usr/
    ├── bin/
    │   └── Daqster                 ← главен изпълним файл
    ├── lib/
    │   ├── libQt*.so.*             ← Qt библиотеки
    │   ├── libicu*.so.*            ← ICU библиотеки
    │   ├── plugins/                ← Qt плъгини
    │   ├── qml/                    ← QML модули
    │   └── daqster/
    │       └── plugins/            ← Daqster плъгини
    └── share/
        ├── applications/           ← .desktop файл
        └── icons/                  ← икона
```

**AppRun настройва environment при стартиране:**

| Променлива | Стойност |
|---|---|
| `LD_LIBRARY_PATH` | `usr/lib/` |
| `QT_PLUGIN_PATH` | `usr/lib/plugins/` |
| `QML2_IMPORT_PATH` | `usr/lib/qml/` |
| `DAQSTER_PLUGIN_DIR` | `usr/lib/daqster/plugins/` |
| `XDG_CONFIG_HOME` | `~/.config/daqster` |



## 11. Future Enhancements

### 1. Plugin Management
- GUI за управление на плъгини
- Plugin marketplace
- Автоматично обновяване на плъгини

### 2. Performance
- Plugin preloading
- Memory optimization
- Startup time improvement

### 3. Security
- Plugin sandboxing
- Code signing
- Permission system

## Заключение

Daqster използва модулна архитектура, която позволява лесно разширяване и поддръжка. Системата за плъгини е проектирана да бъде безопасна, ефективна и лесна за използване. Build системата поддържа както локална разработка, така и CI/CD за автоматизирано разпространение.
