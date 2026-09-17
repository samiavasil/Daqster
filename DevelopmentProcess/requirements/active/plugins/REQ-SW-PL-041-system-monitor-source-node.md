# REQ-SW-PL-041: System Monitor Source Node (Linux /proc + /sys telemetry)

- **Статус:** ACTIVE
- **Приоритет:** High
- **Отговорник (роля):** Ivan (Implementation)
- **Дата:** 2026-09-05
- **Родител:** REQ-SW-PL-014
- **Зависи от:** REQ-SW-PL-022

## Описание

System Monitor source нод в `demo_nodeditor_nodes` plugin-а: чете Linux системна
телеметрия от `/proc` и `/sys` (CPU usage, RAM, температура, мрежа) и я емитира
като `SampledData` (domain="system") за визуализация в DaqDisplayNode или DSP
процесинг.

### 1. Data type решение

SampledData с `domain="system"` — каналите описват различни метрики:

- `SampledStreamDescriptor`: `sampleRate` = 1/pollInterval (Hz),
  `channels` = `[{"cpu_percent", FLOAT32}, {"ram_percent", FLOAT32}, {"cpu_temp_c", FLOAT32}, {"net_rx_kbps", FLOAT32}, {"net_tx_kbps", FLOAT32}]`,
  `domain` = "system", `deviceId` = "sysmon",
  `sourceName` = "Linux System Monitor".

DaqDisplayNode консумира нода без промени — waveform на CPU/RAM/temp каналите
работи веднага.

### 2. Архитектура

- **`SystemMonitorEngine`** — чете `/proc/stat` (CPU delta), `/proc/meminfo` (RAM),
  `/sys/class/hwmon/*/temp*_input` (температури), `/proc/net/dev` (мрежа).
  Timer-based (QTimer), не worker thread — четенето на /proc е бързо (<1ms).
- **`SystemMonitorModel`** (`NodeDelegateModel`) — 1 изходен порт
  (`SampledData` "sample"), connection-count gating, обвива текущите стойности
  в `shared_ptr<SampledData>` и емитира `dataUpdated(0)`.
- **`SystemMonitorWidget`** — UI: polling interval (0.1s–5s, default 1s),
  чекбокси за метрики (CPU, RAM, Temp, Network), Start/Stop, статус.

### 3. Платформена поддръжка

Първа версия — **Linux-only** (четене от `/proc` и `/sys`). Връзката се охранява
чрез `HAVE_SYSTEM_MONITOR` (CMake: `if(NOT WIN32)`). Windows поддръжка (PDH/WMI)
е бъдещо изискване.

### 4. Метрики

- **CPU%**: диференчно четене на `/proc/stat` (user+system+idle delta / total delta)
- **RAM%**: `(MemTotal - MemAvailable) / MemTotal * 100` от `/proc/meminfo`
- **CPU temp**: първият намерен `temp*_input` от `/sys/class/hwmon/*/` (милиградуси → °C)
- **Net RX/TX kbps**: диференчно четене на `/proc/net/dev` (bytes delta / interval)

## Acceptance Criteria

- [ ] 1. **Регистрация.** `SystemMonitorModel` се регистрира в
       `DemoNodeEditorNodesObject::registerNodes()` като `"Daq/Sources"`
       (охранен с `#ifdef HAVE_SYSTEM_MONITOR`).
- [ ] 2. **Platform guard.** `HAVE_SYSTEM_MONITOR` се дефинира в CMake чрез
       `if(NOT WIN32)` — на Windows build-ът минава без нода.
- [ ] 3. **Config UI.** `SystemMonitorWidget`: polling interval (0.1–5s), чекбокси
       за CPU/RAM/Temp/Network, Start/Stop, статус.
- [ ] 4. **Метрики.** Четене на `/proc/stat` (CPU delta), `/proc/meminfo` (RAM),
       `/sys/class/hwmon/*/temp*_input` (температура), `/proc/net/dev` (мрежа).
- [ ] 5. **SampledData domain="system".** Дескрипторът: 5 канала FLOAT32,
       `sampleRate` = 1/interval, `domain` = "system"; консумируеми от DaqDisplayNode.
- [ ] 6. **Чисто спиране.** Stop/деструктор: QTimer stop + thread join (ако има worker)
       — без crash при затваряне на графа.
- [ ] 7. **Верификация.** Qt5 + Qt6 builds PASS; ctest 11/11 green;
       headless smoke (offscreen, без хардуер — нодът се създава, UI-то работи,
       липсата на метрики не крашва); hardware smoke (на Linux — метриките се четат
       коректно от /proc и /sys); unit тестове ОТЛОЖЕНИ (стоящата инструкция „НОВИ
       ТЕСТОВЕ СТОП" от 2026-08-13).

## Проследимост

- **Коммити:** `997eb62` (docs: REQ file), *(feat: System Monitor node — this branch)*
- **Код:** `src/plugins/demo_nodeditor_nodes/Sources/SystemMonitor/`
  (`SystemMonitorEngine.{h,cpp}`, `SystemMonitorModel.{h,cpp}`,
  `SystemMonitorWidget.{h,cpp}`), `DemoNodeEditorNodesObject.cpp`, `CMakeLists.txt`
- **Документация:** `docs/plugins/demo_nodeditor_nodes/README.md`,
  traceability matrix, CHANGELOG

## Бележка

Изискването е създадено по решение на потребителя (2026-09-05) след проучването в
`DevelopmentProcess/plans/daq-node-candidates-2026-09-05.md` (секция A — System
Monitor). Нодът е Linux-only v1, Windows поддръжка е бъдещо изискване. Моделът на
data type е SampledData с domain="system" (по аналогия на domain="iq" от PL-040).
Unit тестове са ОТЛОЖЕНИ по стоящата инструкция „НОВИ ТЕСТОВЕ СТОП".
