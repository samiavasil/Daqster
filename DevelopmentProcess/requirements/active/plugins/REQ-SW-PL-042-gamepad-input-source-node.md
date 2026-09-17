# REQ-SW-PL-042: Gamepad Input Source Node (Linux joystick API)

- **Статус:** ACTIVE
- **Приоритет:** High
- **Отговорник (роля):** Ivan (Implementation)
- **Дата:** 2026-09-05
- **Родител:** REQ-SW-PL-014
- **Зависи от:** REQ-SW-PL-022

## Описание

Gamepad input source нод в `demo_nodeditor_nodes` plugin-а: чете оси и бутони
от USB gamepad през Linux joystick API (`/dev/input/js0`) и ги емитира като
`SampledData` (domain="gamepad") за визуализация или контрол.

### 1. Хардуер

ShanWan USB gamepad (VID:081f, PID:e001) — бюджетен USB геймпад, 4 оси
(X, Y, Z, Rz), 8 бутона. Поддържа се natивно от Linux joystick driver
(`joydev` модул). Устройството се появява като `/dev/input/js0`.

### 2. Data type решение

SampledData с `domain="gamepad"`:

- `SampledStreamDescriptor`: `sampleRate` = poll rate (60 Hz),
  `channels` = `[{"axis_x", FLOAT32}, {"axis_y", FLOAT32}, {"axis_z", FLOAT32},
  {"axis_rz", FLOAT32}, {"button_a", FLOAT32}, {"button_b", FLOAT32},
  {"button_x", FLOAT32}, {"button_y", FLOAT32}, {"button_lb", FLOAT32},
  {"button_rb", FLOAT32}, {"button_back", FLOAT32}, {"button_start", FLOAT32}]`,
  `domain` = "gamepad", `deviceId` = "gamepad",
  `sourceName` = "USB Gamepad".

Осите са нормализирани от `int16 [-32767, 32767]` до `float [-1.0, 1.0]`.
Бутоните са `0.0` (натиснат) или `1.0` (отпуснат) — по Linux joystick конвенцията.

### 3. Архитектура

- **`GamepadEngine`** — Linux joystick API: `open("/dev/input/js0", O_RDONLY)`,
  non-blocking `read()` на `struct js_event`, QTimer за polling (60 Hz).
  Съхранява текущото състояние на осите и бутоните.
- **`GamepadModel`** (`NodeDelegateModel`) — 1 изходен порт (`SampledData` "sample"),
  connection-count gating, обвива текущото състояние в `SampledData` и
  емитира `dataUpdated(0)`.
- **`GamepadWidget`** — UI: axis display (4 стойности), button state (8 индикатора),
  Start/Stop, device status, axes mapping config.

## Acceptance Criteria

- [ ] 1. **Регистрация.** `GamepadModel` се регистрира в
       `DemoNodeEditorNodesObject::registerNodes()` като `"Daq/Sources"`
       (охранен с `#ifdef HAVE_GAMEPAD`).
- [ ] 2. **Platform guard.** `HAVE_GAMEPAD` се дефинира в CMake чрез
       `if(NOT WIN32)` — на Windows build-ът минава без нода.
- [ ] 3. **Config UI.** `GamepadWidget`: device path (default `/dev/input/js0`),
       axis display (4 стойности), button state (8 индикатора), Start/Stop,
       статус.
- [ ] 4. **Joystick API.** `GamepadEngine` чете `/dev/input/js0` с non-blocking
       `read()` + QTimer polling (60 Hz); осите нормализирани [-1.0, 1.0],
       бутоните 0.0/1.0.
- [ ] 5. **SampledData domain="gamepad".** Дескрипторът: 12 канала FLOAT32
       (4 оси + 8 бутона), `sampleRate` = 60, `domain` = "gamepad";
       консумируеми от DaqDisplayNode.
- [ ] 6. **Чисто спиране.** Stop/деструктор: close(fd) + QTimer stop —
       без crash при затваряне на графа.
- [ ] 7. **Верификация.** Qt5 + Qt6 builds PASS; ctest 11/11 green;
       headless smoke (offscreen — нодът се създава, липсата на /dev/input/js0
       се репортва в статус, без crash); hardware smoke (ако gamepad-ът е
       свързан — осите се четат, бутоните работят); unit тестове ОТЛОЖЕНИ.

## Проследимост

- **Коммити:** —
- **Код:** `src/plugins/demo_nodeditor_nodes/Sources/Gamepad/`
  (`GamepadEngine.{h,cpp}`, `GamepadModel.{h,cpp}`, `GamepadWidget.{h,cpp}`),
  `DemoNodeEditorNodesObject.cpp`, `CMakeLists.txt`

## Бележка

Изискването е създадено по решение на потребителя (2026-09-05) след проучването в
`DevelopmentProcess/plans/daq-node-candidates-2026-09-05.md` (секция C — Gamepad).
Linux-only v1 (Linux joystick API). Хардуерът е ShanWan USB gamepad (081f:e001).
Unit тестове са ОТЛОЖЕНИ по стоящата инструкция „НОВИ ТЕСТОВЕ СТОП".