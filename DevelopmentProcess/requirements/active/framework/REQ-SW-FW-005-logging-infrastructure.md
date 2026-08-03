# REQ-SW-FW-005: Logging Infrastructure

- **Статус:** DONE
- **Приоритет:** P1
- **Отговорник (роля):** Implementation
- **Дата:** 2026-08-02
- **Родител:** —
- **Зависи от:** —

## Описание

Ретроспективно изискване за `LogManager` в `frame_work/base`: глобален
Qt message handler, който форматира и насочва логовете към конзола и файл, с
поддръжка на категории, нива и `instanceId` за multi-instance диагностика.

## Acceptance Criteria

- [x] 1. `daqsterMessageHandler` (инсталиран през `qInstallMessageHandler`) записва
       в конзола (при `setConsoleEnabled` и праг `consoleLogLevel`) и във файл
       (`setLogFile`), с формат `[timestamp] [LEVEL] [PID[:instanceId]] msg`.
- [x] 2. `instanceId` участва в логовете: `LogManager::setInstanceId()` задава
       глобалния `s_instanceId`, който се добавя към source полето на всеки ред
       (multi-instance диагностика).
- [x] 3. Категорийният контрол (enable/disable per category, master toggle)
       се прилага чрез `QLoggingCategory::setFilterRules()` и се emit-ва чрез
       `filterRulesChanged`.

## Проследимост

- **Коммити:** `5eed6d6` (feat: configurable log level threshold, console enable/disable), `bc18fa4` (feat: auto-registration of log categories, master toggle fix, pass filter rules)
- **Код:** `src/frame_work/base/src/LogManager.cpp`, `include/LogManager.h`
- **Тестове:** Qt5 + Qt6 builds
