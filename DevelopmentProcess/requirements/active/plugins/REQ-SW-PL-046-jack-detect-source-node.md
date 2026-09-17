# REQ-SW-PL-046: Jack-detect Source Node (HDA jack events)

- **Статус:** ACTIVE
- **Приоритет:** Medium
- **Отговорник (роля):** Ivan (Implementation)
- **Дата:** 2026-09-05
- **Родител:** REQ-SW-PL-014
- **Зависи от:** REQ-SW-PL-022

## Описание

Jack-detect source нод в `demo_nodeditor_nodes` plugin-а: следи състоянието
на аудио jack-овете (headphone, mic, line-in) през HDA jack файловете в
`/proc/asound/card*/codec#*/jack*` и емитира промените като `SampledData`
(domain="jack").

### 1. Източник на данни

Linux HDA драйверът публикува jack състоянието в `/proc/asound/card*/codec#*/jack*`:

```
Pin 0x12 (Internal Mic): present = No
Pin 0x14 (Speaker): present = Yes
Pin 0x21 (Headphone): present = No
```

Нодът poll-ва тези файлове (QTimer, default 500ms) и детектира промени.

### 2. Data type решение

SampledData с `domain="jack"`:

- `SampledStreamDescriptor`: `sampleRate` = 1/pollInterval,
  `channels` = динамични — по един FLOAT32 канал за всеки открит jack
  (напр. `{"headphone", FLOAT32}`, `{"mic", FLOAT32}`),
  `domain` = "jack", `deviceId` = "hda",
  `sourceName` = "HDA Jack Detect".

Стойностите са `0.0` (unplugged) или `1.0` (plugged). При промяна на jack
състояние нодът емитира нов `SampledData` (event-driven).

### 3. Архитектура

- **`JackDetectEngine`** — сканира `/proc/asound/card*/codec#*/jack*`, парсва
  jack имената и състоянията, QTimer polling (default 500ms), детектира промени.
- **`JackDetectModel`** (`NodeDelegateModel`) — 1 изходен порт (`SampledData`
  "sample"), connection-count gating, емитира `dataUpdated(0)` при промяна.
- **`JackDetectWidget`** — UI: polling interval, Start/Stop, статус
  (списък на jack-овете и състоянията им).

## Acceptance Criteria

- [ ] 1. **Регистрация.** `JackDetectModel` като `"Daq/Sources"` в
       `registerNodes()` (охранен с `#ifdef HAVE_JACK_DETECT`).
- [ ] 2. **Platform guard.** `HAVE_JACK_DETECT` се дефинира в CMake чрез
       `if(NOT WIN32)` — на Windows build-ът минава без нода.
- [ ] 3. **Config UI.** `JackDetectWidget`: polling interval (0.1–5s),
       Start/Stop, статус (списък на jack-овете).
- [ ] 4. **HDA сканиране.** Четене на `/proc/asound/card*/codec#*/jack*`,
       парсване на jack имена + състояния, детекция на промени.
- [ ] 5. **SampledData domain="jack".** Дескрипторът: динамични FLOAT32 канали
       (по един на jack), `domain` = "jack"; консумируеми от DaqDisplayNode.
- [ ] 6. **Чисто спиране.** Stop/деструктор: QTimer stop — без crash.
- [ ] 7. **Верификация.** Qt5 + Qt6 builds PASS; ctest green;
       headless smoke (offscreen — без jack файлове нодът репортва празен
       статус, без crash); hardware smoke (ALC294 — jack файловете се четат);
       unit тестове ОТЛОЖЕНИ.

## Проследимост

- **Коммити:** —
- **Код:** `src/plugins/demo_nodeditor_nodes/Sources/JackDetect/`
  (`JackDetectEngine.{h,cpp}`, `JackDetectModel.{h,cpp}`,
  `JackDetectWidget.{h,cpp}`), `DemoNodeEditorNodesObject.cpp`, `CMakeLists.txt`

## Бележка

Изискването е създадено по решение на потребителя (2026-09-05) след проучването в
`DevelopmentProcess/plans/daq-node-candidates-2026-09-05.md` (секция G — Jack
detect). Linux-only v1 (HDA jack файлове в /proc/asound). Хардуерът е ALC294
codec. Unit тестове са ОТЛОЖЕНИ по стоящата инструкция „НОВИ ТЕСТОВЕ СТОП".