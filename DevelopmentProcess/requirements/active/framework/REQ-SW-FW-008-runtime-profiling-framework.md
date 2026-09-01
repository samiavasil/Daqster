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

- [x] 1. **Модулна скелет.** Нов модул `src/frame_work/base/src/perf/` (брат на
      `platform/`, `process/`, `gui/`) съдържа `PerfProfiler.h` (header-only за
      hot path) + `PerfProfiler.cpp`. Целият API е в `namespace Daqster::Perf`.
- [x] 2. **`Domain`.** `static Domain &get(const char*)` — thread-safe
      get-or-create регистър по стринг име (`"video"`, `"audio"`, `"tensor"`…);
      `bool enabled() const` — atomic **relaxed** load; `void setEnabled(bool)`;
      `void record(const char *stage, std::int64_t ns)` — **no-op** при изключен
      домейн (нулев разход в hot path); `void flush()` — агрегира и log-ва през
      `qCDebug(lcPerf)` + reset на статистиките.
- [x] 3. **`Scope` + `Stopwatch`.** `Scope` — RAII таймер за синхронни блокове
      (записва elapsed ns при разрушаване; нулев разход при изключен домейн —
      guard преди стартиране на timer-а). `Stopwatch` — `std::int64_t mark()`
      връща ns от последния mark/start, за async/event мерене (inter-frame gap,
      decode vs present).
- [x] 4. **`RollingStats`.** Фиксиран ring buffer с фиксиран капацитет, изчислява
      avg/min/max; буферът е pre-allocated — **без heap алокация в hot path**.
- [x] 5. **Макрота + compile-time opt-out.** `PERF_SCOPE(dom, stage)` и
      `PERF_ENABLED(dom)`; при `DAQSTER_ENABLE_PERF=OFF` се дефинират като
      `((void)0)` и `false` — никой символ от `Daqster::Perf` не се генерира
      (нулев binary footprint).
- [x] 6. **Двустепенно изключване.** Compile-time (`DAQSTER_ENABLE_PERF` CMake
      option, default ON) + runtime (atomic flag, live toggle през
      `Domain::setEnabled`).
- [x] 7. **CMake интеграция.** В `src/frame_work/CMakeLists.txt`:
      `option(DAQSTER_ENABLE_PERF "..." ON)`; двата файла в `FRAME_WORK_SOURCES`;
      `target_compile_definitions(frame_work PUBLIC DAQSTER_ENABLE_PERF=...)`;
      `base/src/perf` добавено в PUBLIC include dirs на `frame_work`.
- [x] 8. **Лог категория.** `lcPerf` = `"daqster.perf"` добавена в
      `LogCategories.{h,cpp}` с auto-registration (за flush агрегатите).

## Проследимост

- **Коммити:** `6ad8e89` (feat: PerfProfiler module — Domain/Scope/Stopwatch/
  RollingStats), `2c31b4a` (feat: lcPerf logging category + CMake integration),
  `52a620c` (test: perf_profiler_tests)
- **Код:** `src/frame_work/base/src/perf/PerfProfiler.h`,
  `src/frame_work/base/src/perf/PerfProfiler.cpp`,
  `src/frame_work/base/src/include/LogCategories.h`,
  `src/frame_work/base/src/LogCategories.cpp`,
  `src/frame_work/CMakeLists.txt`
- **Тестове:** `tests/framework/perf/perf_profiler_tests` — 19/19 PASS
  (RollingStats 6, Domain 7, Stopwatch/Scope 6); Qt5 (5.15.2) + Qt6 (6.9.2)
  builds PASS + ctest 9/9 green (и двете); compile-time opt-out проверка —
  `-DDAQSTER_ENABLE_PERF=OFF` build PASS, макротата се редуцират до
  `((void)0)`/`false` (nm проверка: нула референции към `Daqster::Perf`
  символи).

## Бележка

Изискването е създадено **преди** имплементацията по одобрения от потребителя
дизайн. Процесната клауза "branch per work item" (AGENTS.md) важи — работата се
върши на нов branch `feat/REQ-SW-FW-008-runtime-profiling`.
