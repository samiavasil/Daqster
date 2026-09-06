# REQ-SW-PL-045: GPU Monitor Source Node (NVIDIA NVML)

- **Статус:** ACTIVE
- **Приоритет:** Medium
- **Отговорник (роля):** Ivan (Implementation)
- **Дата:** 2026-09-05
- **Родител:** REQ-SW-PL-014
- **Зависи от:** REQ-SW-PL-022

## Описание

GPU Monitor source нод в `demo_nodeditor_nodes` plugin-а: чете NVIDIA GPU
телеметрия през NVML (NVIDIA Management Library) и я емитира като `SampledData`
(domain="gpu") за визуализация в DaqDisplayNode.

### 1. Метрики

- **GPU utilization %** — `nvmlDeviceGetUtilizationRates()`
- **Memory used %** — `nvmlDeviceGetMemoryInfo()` (used / total)
- **Temperature °C** — `nvmlDeviceGetTemperature(NVML_TEMPERATURE_GPU)`
- **Power draw W** — `nvmlDeviceGetPowerUsage()`
- **Fan speed %** — `nvmlDeviceGetFanSpeed()`
- **Graphics clock MHz** — `nvmlDeviceGetClockInfo(NVML_CLOCK_GRAPHICS)`

### 2. Data type решение

SampledData с `domain="gpu"`:

- `SampledStreamDescriptor`: `sampleRate` = 1/pollInterval,
  `channels` = `[{"gpu_util", FLOAT32}, {"mem_used", FLOAT32},
  {"gpu_temp_c", FLOAT32}, {"power_w", FLOAT32}, {"fan_pct", FLOAT32},
  {"clock_mhz", FLOAT32}]`,
  `domain` = "gpu", `deviceId` = "gpu0",
  `sourceName` = "NVIDIA RTX 3070 Mobile".

### 3. Архитектура

- **`GpuMonitorEngine`** — NVML обвивка: `nvmlInit()`, handle за GPU 0,
  QTimer polling (default 1s), `nvmlShutdown()` при stop.
- **`GpuMonitorModel`** (`NodeDelegateModel`) — 1 изходен порт (`SampledData`
  "sample"), connection-count gating, обвива метриките в `SampledData` и
  емитира `dataUpdated(0)`.
- **`GpuMonitorWidget`** — UI: polling interval, Start/Stop, статус.

### 4. NVML зависимост (опционална, моделът на libiio/OpenCV)

`find_library(NVML_LIBRARY nvidia-ml)` + `find_path(NVML_INCLUDE_DIR nvml.h)`.
Без NVML → build-ът минава без нода (HAVE_NVML guard).

## Acceptance Criteria

- [ ] 1. **Регистрация.** `GpuMonitorModel` като `"Daq/Sources"` в
       `registerNodes()` (охранен с `#ifdef HAVE_NVML`).
- [ ] 2. **NVML dependency.** `find_library` + `find_path` в CMake; опционална
       компилация (без NVML → build-ът минава без нода); Qt5 И Qt6 builds PASS.
- [ ] 3. **Config UI.** `GpuMonitorWidget`: polling interval (0.1–5s),
       Start/Stop, статус (GPU name, metrics).
- [ ] 4. **Метрики.** Четене на utilization/memory/temperature/power/fan/clock
       през NVML API.
- [ ] 5. **SampledData domain="gpu".** Дескрипторът: 6 канала FLOAT32,
       `sampleRate` = 1/interval, `domain` = "gpu"; консумируеми от DaqDisplayNode.
- [ ] 6. **Чисто спиране.** Stop/деструктор: QTimer stop + `nvmlShutdown()` —
       без crash.
- [ ] 7. **Верификация.** Qt5 + Qt6 builds PASS; ctest 11/11 green;
       headless smoke (offscreen — без NVML нодът не се компилира, с NVML се
       създава без crash); hardware smoke (RTX 3070 — метриките се четат
       коректно); unit тестове ОТЛОЖЕНИ.

## Проследимост

- **Коммити:** —
- **Код:** `src/plugins/demo_nodeditor_nodes/Sources/GpuMonitor/`
  (`GpuMonitorEngine.{h,cpp}`, `GpuMonitorModel.{h,cpp}`,
  `GpuMonitorWidget.{h,cpp}`), `DemoNodeEditorNodesObject.cpp`, `CMakeLists.txt`

## Бележка

Изискването е създадено по решение на потребителя (2026-09-05) след проучването в
`DevelopmentProcess/plans/daq-node-candidates-2026-09-05.md` (секция B — GPU
Monitor). NVML е опционална зависимост по модела на libiio/OpenCV. Хардуерът е
NVIDIA RTX 3070 Mobile (laptop). Unit тестове са ОТЛОЖЕНИ по стоящата инструкция
„НОВИ ТЕСТОВЕ СТОП".
