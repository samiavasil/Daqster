# REQ-SW-PL-043: File Record + File Playback DAQ Nodes

- **Статус:** ACTIVE
- **Приоритет:** High
- **Отговорник (роля):** Ivan (Implementation)
- **Дата:** 2026-09-05
- **Родител:** REQ-SW-PL-014
- **Зависи от:** REQ-SW-PL-022

## Описание

Двойка нодове за запис и възпроизвеждане на `SampledData` потоци на диск:

- **File Record** (Sink) — консумира `SampledData` от графа и го записва на диск
  като raw bytes + JSON sidecar (метаданни);
- **File Playback** (Source) — чете записания файл + sidecar, реконструира
  `SampledData` и го емитира на записания sample rate.

### 1. Файлов формат

Два файла с еднаква база:

- `*.sdf` — raw interleaved sample bytes (точно както идват от източника);
- `*.sdf.json` — JSON sidecar с `SampledStreamDescriptor`:
  `{"sampleRate": 2400000, "channels": [{"name":"I","type":"INT16"},
  {"name":"Q","type":"INT16"}], "domain": "iq", "deviceId": "plutosdr",
  "sourceName": "PlutoSky 7020-SDR", "endianness": "LittleEndian"}`.

Форматът е прост и debuggable: raw bytes + JSON метаданни. Няма custom binary
header — sidecar-ът е човешки четим.

### 2. Архитектура

- **`FileRecordModel`** (Sink, `NodeDelegateModel`) — 1 входен порт
  (`SampledData` "sample"); при всяко пристигане на данни записва raw bytes
  във файла; при Start записва sidecar JSON; при Stop финализира файла.
- **`FileRecordWidget`** — UI: файлов път, Start/Stop запис, статус (bytes written).
- **`FilePlaybackModel`** (Source, `NodeDelegateModel`) — 1 изходен порт
  (`SampledData` "sample"); чете файла + sidecar, реконструира дескриптора,
  емитира данните на записания sample rate (QTimer-базирано темпо).
- **`FilePlaybackWidget`** — UI: файлов път, Play/Stop, статус (position/duration).

### 3. Темпо на Playback

QTimer с интервал = chunkSize / sampleRate. Всеки тик емитира един chunk
(напр. 4096 семпла). Ако sampleRate е 2.4 MSPS и chunk е 4096 → интервал ~1.7ms.
За ниски sample rates (System Monitor 1 Hz) chunk-ът е 1 семпъл → интервал 1s.

## Acceptance Criteria

- [ ] 1. **Регистрация.** `FileRecordModel` като `"Daq/Sinks"` и
       `FilePlaybackModel` като `"Daq/Sources"` в
       `DemoNodeEditorNodesObject::registerNodes()`.
- [ ] 2. **File Record.** Приема `SampledData` на входния порт, записва raw bytes
       във `*.sdf` + JSON sidecar `*.sdf.json`; Start/Stop запис; статус с
       bytes written.
- [ ] 3. **File Playback.** Чете `*.sdf` + `*.sdf.json`, реконструира
       `SampledStreamDescriptor`, емитира `SampledData` на записания sample rate;
       Play/Stop; статус position/duration.
- [ ] 4. **Round-trip.** Запис на поток от PlutoSDR/System Monitor → Playback →
       DaqDisplay показва същите данни (дескрипторът се възстановява коректно).
- [ ] 5. **Чисто спиране.** Stop/деструктор: файловете се затварят коректно,
       QTimer-ът спира — без crash.
- [ ] 6. **Верификация.** Qt5 + Qt6 builds PASS; ctest 11/11 green;
       headless smoke (offscreen — нодовете се създават, липсата на файл се
       репортва в статус, без crash); round-trip smoke (запис на генериран
       поток → Playback → валидация на bytes); unit тестове ОТЛОЖЕНИ.

## Проследимост

- **Коммити:** —
- **Код:** `src/plugins/demo_nodeditor_nodes/Sinks/FileRecord/` и
  `src/plugins/demo_nodeditor_nodes/Sources/FilePlayback/`
  (`FileRecordModel.{h,cpp}`, `FileRecordWidget.{h,cpp}`,
  `FilePlaybackModel.{h,cpp}`, `FilePlaybackWidget.{h,cpp}`),
  `DemoNodeEditorNodesObject.cpp`, `CMakeLists.txt`

## Бележка

Изискването е създадено по решение на потребителя (2026-09-05) след проучването в
`DevelopmentProcess/plans/daq-node-candidates-2026-09-05.md` (секция D — File
Record/Playback). Форматът е raw bytes + JSON sidecar (без custom binary header).
Тези нодове са предпоставка за TX пътя на PlutoSDR (записан сигнал → Playback →
TX). Unit тестове са ОТЛОЖЕНИ по стоящата инструкция „НОВИ ТЕСТОВЕ СТОП".
