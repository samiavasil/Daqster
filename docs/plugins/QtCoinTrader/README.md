# QtCoinTrader Plugin

Родител: [Plugins](../README.md) | [Architecture](../../Architecture/README.md)

## Преглед

APPLICATION_PLUGIN за cryptocurrency trading, използващ QML за потребителски интерфейс. Демонстрира интеграция с QtRest библиотеката за REST API комуникация и QtCharts за визуализация.

**Локация:** `src/plugins/QtCoinTrader/`
**Тип:** APPLICATION_PLUGIN (самостоятелно GUI приложение)

## Архитектура

```
QtCoinTrader/
├── QtCoinTraderInterface.{h,cpp}      # QPluginInterface — factory
├── QtCoinTraderInterface.json          # Plugin metadata
├── QtCoinTraderPluginObject.{h,cpp}    # QBasePluginObject — lifecycle
├── RestApiTester.{h,cpp}              # REST API testing dialog (QDialog)
├── RequestForm.{h,cpp}                # HTTP request form (GET/POST/PUT/DELETE)
├── utils/
│   └── RandData.{h,cpp}              # Random data generator за QML
├── qml/                               # QML файлове за UI
│   └── qml_example/
├── icons/                             # Иконки
├── assets/                            # Статични ресурси
├── QtCoinTrader.qrc                   # Qt Resource файл
└── CMakeLists.txt                     # Build конфигурация
```

## Имплементирани интерфейси

### QPluginInterface ( frame_work )
```
DaqsterTemplateInterface → QPluginInterface
  ├── Q_PLUGIN_METADATA(IID "Daqster.PlugIn.QPluginInterface")
  ├── CreatePluginInternal() → създава QtCoinTraderPluginObject
  └── PLUGIN_NAME = "QtCoinTrader"
```

### QBasePluginObject ( frame_work )
```
QtCoinTraderPluginObject → QBasePluginObject
  ├── Initialize()   → инициализира QtRest, QML engine, зарежда QML UI
  └── DeInitialize() → унищожава прозореца
```

## Основни класове

| Класс | Описание |
|-------|----------|
| `QtCoinTraderPluginObject` | Основен plugin object — животен цикъл и инициализация |
| `RestApiTester` | QDialog за тестване на REST API заявки с QWebEngineView |
| `RequestForm` | QWidget за конструиране на HTTP заявки (GET/POST/PUT/DELETE/HEAD/OPTIONS/PATCH) |
| `RandData` | QObject генериращ случайни данни за QML demo |

## Зависимости

- **Qt::Core, Qt::Gui** — основни Qt модули
- **Qt::Qml** — QML engine за UI
- **Qt::QuickControls2** — Material design UI контроли
- **Qt::Charts** — графична визуализация
- **Qt::Network** — HTTP комуникация
- **Qt::OpenGL** — rendering
- **Qt::WebSockets** — WebSocket комуникация
- **qtrest_lib** — REST API клиент library
- **OpenSSL::SSL, OpenSSL::Crypto** — криптография за HTTPS
- **frame_work** — Daqster core framework

## Как работи

1. Plugin-ът се стартира като отделен процес от Daqster
2. `Initialize()` регистрира QML типове и зарежда QML файл
3. Използва QtRest library за REST API комуникация с cryptocurrency борси
4. Material design UI чрез QQuickStyle
5. Визуализация на данни чрез QtCharts

## Свързана документация

- [Plugins](../README.md) — общ преглед на плъгините
- [QtRest Porting](../../porting/QtRest_Qt6_Porting.md) — Qt6 porting бележки за QtRest
- [Upstream Management](../../operations/UpstreamManagement.md) — upstream dependency management
