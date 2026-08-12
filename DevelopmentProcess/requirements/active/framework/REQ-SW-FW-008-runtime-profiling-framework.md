# REQ-SW-FW-008: Lightweight Runtime Profiling Framework

- **Статус:** ACTIVE
- **Приоритет:** High
- **Отговорник (роля):** Ivan (Implementation)
- **Дата:** 2026-08-12
- **Родител:** REQ-SW-FW-005 (архив: archive/framework/REQ-SW-FW-005-logging-infrastructure.md)
- **Зависи от:** REQ-SW-FW-005 (архив: archive/framework/REQ-SW-FW-005-logging-infrastructure.md)

## Описание

Съществуващата logging инфраструктура (REQ-SW-FW-005) покрива диагностичните
логове, но frame_work-ът няма механизъм за runtime профилиране на
производителността. Това изискване добавя **генеричен, преизползваем** runtime
profiling модул в ядрото на `frame_work`-а: леки таймери и статистики с **нулев
разход при изключване**, осигурен двустепенно — compile-time
(`DAQSTER_ENABLE_PERF`, CMake option) + runtime (atomic flag, live toggle).
Модулът е framework-level: първият консуматор е видео пайплайнът (REQ-SW-PL-027),
но всеки плъгин може да го ползва.

## Acceptance Criteria

- [ ] 1. **Модулна скелет.** Нов модул `src/frame_work/base/src/perf/` (брат на
      `platform/`, `process/`, `gui/`) съдържа `PerfProfiler.h` (header-only за
      hot path) + `PerfProfiler.cpp`. Целият API е в `namespace Daqster::Perf`.
- [ ] 2. **`Domain`.** `static Domain &get(const char*)` — thread-safe
      get-or-create регистър по стринг име (`"video"`, `"audio"`, `"tensor"`…);
      `bool enabled() const` — atomic **relaxed** load; `void setEnabled(bool)`;
      `void record(const char *stage, std::int64_t ns)` — **no-op** при изключен
      домейн (нулев разход в hot path); `void flush()` — агрегира и log-ва през
      `qCDebug(lcPerf)` + reset на статистиките.
- [ ] 3. **`Scope` + `Stopwatch`.** `Scope` — RAII таймер за синхронни блокове
      (записва elapsed ns при разрушаване; нулев разход при изключен домейн —
      guard преди стартиране на timer-а). `Stopwatch` — `std::int64_t mark()`
      връща ns от последния mark/start, за async/event мерене (inter-frame gap,
      decode vs present).
- [ ] 4. **`RollingStats`.** Фиксиран ring buffer с фиксиран капацитет, изчислява
      avg/min/max; буферът е pre-allocated — **без heap алокация в hot path**.
- [ ] 5. **Макрота + compile-time opt-out.** `PERF_SCOPE(dom, stage)` и
      `PERF_ENABLED(dom)`; при `DAQSTER_ENABLE_PERF=OFF` се дефинират като
      `((void)0)` и `false` — никой символ от `Daqster::Perf` не се генерира
      (нулев binary footprint).
- [ ] 6. **Двустепенно изключване.** Compile-time (`DAQSTER_ENABLE_PERF` CMake
      option, default ON) + runtime (atomic flag, live toggle през
      `Domain::setEnabled`).
- [ ] 7. **CMake интеграция.** В `src/frame_work/CMakeLists.txt`:
      `option(DAQSTER_ENABLE_PERF "..." ON)`; двата файла в `FRAME_WORK_SOURCES`;
      `target_compile_definitions(frame_work PUBLIC DAQSTER_ENABLE_PERF=...)`;
      `base/src/perf` добавено в PUBLIC include dirs на `frame_work`.
- [ ] 8. **Лог категория.** `lcPerf` = `"daqster.perf"` добавена в
      `LogCategories.{h,cpp}` с auto-registration (за flush агрегатите).

## Проследимост

- **Коммити:** — (след имплементация)
- **Код:** `src/frame_work/base/src/perf/PerfProfiler.h`,
  `src/frame_work/base/src/perf/PerfProfiler.cpp`,
  `src/frame_work/base/src/include/LogCategories.h`,
  `src/frame_work/base/src/LogCategories.cpp`,
  `src/frame_work/CMakeLists.txt`
- **Тестове:** план — unit тестове за `RollingStats` (avg/min/max + ring
  overflow), `Domain` регистър (get-or-create, `setEnabled` toggle, no-op при
  off), `Stopwatch`/`Scope` (elapsed > 0); compile-time opt-out проверка (build с
  `DAQSTER_ENABLE_PERF=OFF` без Perf символи). Qt5 (5.15.2) + Qt6 (6.9.2) builds
  PASS.

## Бележка

Изискването е създадено **преди** имплементацията по одобрения от потребителя
дизайн. Процесната клауза "branch per work item" (AGENTS.md) важи — работата се
върши на нов branch `feat/REQ-SW-FW-008-runtime-profiling`.
