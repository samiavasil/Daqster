# REQ-SW-FW-003: Plugin GUI & Debug Console

- **Статус:** DONE
- **Приоритет:** P2
- **Отговорник (роля):** Implementation
- **Дата:** 2026-08-02
- **Родител:** —
- **Зависи от:** REQ-SW-FW-001

## Описание

Ретроспективно изискване за GUI слоя на Plugin Manager-а: `QPluginManagerGui`
(диалог), `QPluginListView` (дърво с плъгини, групирани по тип) и
`DebugConsoleWidget` (настройки на логването — категории, ниво, файл).

## Acceptance Criteria

- [x] 1. `QPluginListView` показва 5-те колони (Name / Enable / Version / Author /
       Description), групира плъгините по тип и обновява списъка при
       `PluginsListChangeDetected` (слот `RefreshView`, QueuedConnection).
- [x] 2. `EnableDisablePlugin` от view-а достига до Plugin Manager: сигналът на
       view-а е свързан към `QPluginManager::EnableDisablePlugin()` (QueuedConnection),
       с поддръжка на три-състоятелен root checkbox (Checked/Unchecked/PartiallyChecked).
- [x] 3. `DebugConsoleWidget` филтрира по категории (checkboxes, групирани от
       `Log::allCategories()`), по level combo (Debug…Fatal) и по файл
       (File Output секция + Browse), като промените се прилагат на
       `LogManager` на живо.

## Проследимост

- **Коммити:** `43c59a2` (refactor: merge frame_work_gui back into frame_work, fix Qt6 build), `5eed6d6` (feat: configurable log level threshold, console enable/disable), `db78014` (fix #110: FRAME_WORKSHARED_EXPORT)
- **Код:** `src/frame_work/base/src/gui/QPluginManagerGui.cpp`, `QPluginListView.{h,cpp}`, `DebugConsoleWidget.{h,cpp}`
- **Тестове:** Qt5 + Qt6 builds
