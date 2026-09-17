# REQ-SW-PL-044: UDP/TCP Source + Sink Network DAQ Nodes

- **Статус:** ACTIVE
- **Приоритет:** Medium
- **Отговорник (роля):** Ivan (Implementation)
- **Дата:** 2026-09-05
- **Родител:** REQ-SW-PL-014
- **Зависи от:** REQ-SW-PL-022

## Описание

Двойка нодове за предаване на `SampledData` потоци по мрежа:

- **UDP/TCP Source** (Source) — слуша на порт, приема datagram-и/поток,
  реконструира `SampledData` и го емитира в графа;
- **UDP/TCP Sink** (Sink) — консумира `SampledData` от графа, сериализира го
  и го изпраща по UDP/TCP.

### 1. Wire формат

Length-prefixed framing, базиран на Qt:

- **UDP:** всеки datagram = `[4-byte magic "MSSD"][4-byte sampleCount]
  [4-byte bytesPerSample][raw sample bytes]`. Дескрипторът (sampleRate, channels)
  се конфигурира в UI-то на двете страни — wire-ът носи само raw bytes.
- **TCP:** същият framing като поток (length-prefixed frames).

### 2. Архитектура

- **`NetworkSourceModel`** (Source, `NodeDelegateModel`) — 1 изходен порт
  (`SampledData` "sample"); `QUdpSocket`/`QTcpServer` слуша на порт;
  при пристигане на frame реконструира `SampledData` с UI-конфигурирания
  дескриптор и емитира `dataUpdated(0)`.
- **`NetworkSourceWidget`** — UI: протокол (UDP/TCP), порт, sampleRate,
  channels (брой + тип), Start/Stop, статус (bytes received).
- **`NetworkSinkModel`** (Sink, `NodeDelegateModel`) — 1 входен порт
  (`SampledData` "sample"); при пристигане на данни сериализира raw bytes
  в frame и изпраща.
- **`NetworkSinkWidget`** — UI: протокол (UDP/TCP), адрес + порт, Start/Stop,
  статус (bytes sent).

### 3. Qt мрежови класове

`QUdpSocket` (UDP), `QTcpServer` + `QTcpSocket` (TCP). Няма външни зависимости —
Qt Network модулът е част от Qt5/Qt6.

## Acceptance Criteria

- [ ] 1. **Регистрация.** `NetworkSourceModel` като `"Daq/Sources"` и
       `NetworkSinkModel` като `"Daq/Sinks"` в `registerNodes()`.
- [ ] 2. **Network Source.** Слуша на порт (UDP/TCP), приема frames,
       реконструира `SampledData` с UI-конфигурирания дескриптор, емитира
       `dataUpdated(0)`; Start/Stop; статус bytes received.
- [ ] 3. **Network Sink.** Приема `SampledData`, сериализира raw bytes в frame,
       изпраща по UDP/TCP; Start/Stop; статус bytes sent.
- [ ] 4. **Round-trip.** Sink → Source на localhost (UDP и TCP): данните се
       възстановяват коректно (bytes match).
- [ ] 5. **Чисто спиране.** Stop/деструктор: сокетите се затварят, таймерите
       спират — без crash.
- [ ] 6. **Верификация.** Qt5 + Qt6 builds PASS; ctest 11/11 green;
       headless smoke (offscreen — нодовете се създават, без crash);
       round-trip smoke (Sink → Source на localhost, UDP + TCP);
       unit тестове ОТЛОЖЕНИ.

## Проследимост

- **Коммити:** —
- **Код:** `src/plugins/demo_nodeditor_nodes/Sources/NetworkSource/` и
  `src/plugins/demo_nodeditor_nodes/Sinks/NetworkSink/`
  (`NetworkSourceModel.{h,cpp}`, `NetworkSourceWidget.{h,cpp}`,
  `NetworkSinkModel.{h,cpp}`, `NetworkSinkWidget.{h,cpp}`),
  `DemoNodeEditorNodesObject.cpp`, `CMakeLists.txt`

## Бележка

Изискването е създадено по решение на потребителя (2026-09-05) след проучването в
`DevelopmentProcess/plans/daq-node-candidates-2026-09-05.md` (секция F — UDP/TCP
Source/Sink). Wire форматът е length-prefixed framing с magic "MSSD"; дескрипторът
се конфигурира в UI-то на двете страни (без out-of-band exchange за v1).
Unit тестове са ОТЛОЖЕНИ по стоящата инструкция „НОВИ ТЕСТОВЕ СТОП".
