# REQ-SW-022: QtCoinTrader Demo Plugin

- **Статус:** DONE
- **Приоритет:** P2
- **Отговорник (роля):** Implementation
- **Дата:** 2026-08-02
- **Родител:** —
- **Зависи от:** REQ-SW-013

## Описание

Ретроспективно изискване за демо плъгина `QtCoinTrader`: QML UI (About.qml),
регистрация на QML типове (RandData + QML модул), REST тестване (RequestForm /
RestApiTester) и липсващата QtCharts интеграция (регистрацията е коментирана).

## Acceptance Criteria

- [x] 1. `Initialize()` регистрира QML типове и зарежда UI:
       `initializeRest()`, `qmlRegisterType<RandData>("com.github.samiavasil.cointrader.randdata", 1, 0, "RandData")`,
       `qmlRegisterModule("com.github.samiavasil.cointrader", 1, 0)` + компоненти
       (MdiArrea, ViewWin, SideBar, ViewModel, SideBarDelegate като QObject),
       `addImportPath("qrc:/qml")` и `engine->load(qrc:/qml/About.qml)` с
       показване/фокусиране на прозореца.
- [x] 2. QtCharts интеграцията не е активна: `qmlRegisterType<QChartView/QChart/...>`
       са закоментирани в `Initialize()` (регистрирани са само RandData и QML
       модул компонентите).
- [x] 3. REST тестване: `RequestForm` използва `QNetworkAccessManager` +
       `QHttpMultiPart` (multipart/form-data POST), а `RestApiTester` е QDialog
       с `QWebEngineView` за API документация/тестове.

## Проследимост

- **Коммити:** `5eed6d6` (feat: configurable log level threshold, console enable/disable), `43c59a2` (refactor: merge frame_work_gui back into frame_work, fix Qt6 build), `f0c0420` (fix: correct QtCoinTrader QML loading under Qt6)
- **Код:** `src/plugins/QtCoinTrader/QtCoinTraderPluginObject.cpp`, `QtCoinTrader.qrc`, `qml/About.qml`, `RequestForm.{h,cpp}`, `RestApiTester.{h,cpp}`, `utils/RandData.h`
- **Тестове:** Qt5 + Qt6 builds (QML smoke зареждане на Qt6)
