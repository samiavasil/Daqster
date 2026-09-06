# REQ-SW-PL-047: pcap Packet Capture Node

- **Статус:** ACTIVE
- **Приоритет:** Medium
- **Отговорник (роля):** Ivan (Implementation)
- **Дата:** 2026-09-06
- **Родител:** REQ-SW-PL-014
- **Зависи от:** REQ-SW-PL-022

## Описание

pcap Packet Capture source нод в `demo_nodeditor_nodes` plugin-а: захваща
мрежови пакети чрез libpcap и ги емитира като `SampledData` (domain="pcap")
за анализ и визуализация.

### 1. Източник на данни

libpcap (Linux: libpcap.so, Windows: WinPcap/Npcap) — стандартна библиотека за
packet capture. Нодът отваря network interface в promiscuous mode, прилага BPF
филтър, и callback-функцията получава пакетите.

### 2. Data type решение

SampledData с `domain="pcap"`:

- `SampledStreamDescriptor`: `sampleRate` = 0 (event-driven, не periodičen),
  `channels` = динамични — по един BYTES канал за всеки пакет (payload),
  `domain` = "pcap", `deviceId` = interface name (напр. "eth0"),
  `sourceName` = "pcap capture".

Всеки пакет се пакетира като `SampledData` с един sample (BYTES) съдържащ
raw packet bytes. Timestamp и packet metadata (дължина, captured length) се
слагат в `SampledData::meta` (QVariantMap).

### 3. Архитектура

- **`PcapEngine`** — libpcap обвивка: `pcap_open_live()`, `pcap_compile()`/
  `pcap_setfilter()` за BPF, `pcap_loop()` в worker thread (QThread),
  `pcap_breakloop()` за спиране, `pcap_close()` при деструктор.
- **`PcapModel`** (`NodeDelegateModel`) — 1 изходен порт (`SampledData`
  "packet"), connection-count gating, приема пакети от Engine чрез
  thread-safe queue, емитира `dataUpdated(0)`.
- **`PcapWidget`** — UI: interface selector (enum от `pcap_findalldevs()`),
  BPF filter text field (напр. "tcp port 80"), Start/Stop бутони, статус
  (packets captured, drops, errors).

### 4. libpcap зависимост (опционална, моделът на NVML)

`find_library(PCAP_LIBRARY pcap)` + `find_path(PCAP_INCLUDE_DIR pcap/pcap.h)`.
Без libpcap → build-ът минава без нода (HAVE_PCAP guard).
На Windows: `find_library(PCAP_LIBRARY wpcap)` + `find_path(PCAP_INCLUDE_DIR pcap.h)`.

## Acceptance Criteria

- [x] 1. **Регистрация.** `PcapModel` като `"Daq/Sources"` в `registerNodes()`
       (охранен с `#ifdef HAVE_PCAP`).
- [x] 2. **libpcap dependency.** `find_library` + `find_path` в CMake;
       опционална компилация (без libpcap → build-ът минава без нода);
       Qt5 И Qt6 builds PASS.
- [x] 3. **Config UI.** `PcapWidget`: interface selector (dropdown от
       `pcap_findalldevs()`), BPF filter text field, Start/Stop, статус
       (packets captured, drops, errors).
- [x] 4. **Packet capture.** Отваряне на interface, BPF компилация, worker
       thread с `pcap_loop()`, thread-safe предаване на пакети към Model.
- [x] 5. **SampledData domain="pcap".** Дескрипторът: BYTES канал за payload,
       `sampleRate` = 0 (event-driven), `domain` = "pcap"; metadata в meta
       (timestamp, caplen, len).
- [x] 6. **Чисто спиране.** Stop/деструктор: `pcap_breakloop()`, thread join,
       `pcap_close()` — без crash, без leaks.
- [ ] 7. **Верификация.** Qt5 + Qt6 builds PASS; ctest green;
       headless smoke (offscreen — без libpcap нодът не се компилира, с libpcap
       се създава без crash); hardware smoke (capture local traffic на lo/eth0);
       unit тестове ОТЛОЖЕНИ.

## Проследимост

- **Коммити:** (pending commit)
- **Код:** `src/plugins/demo_nodeditor_nodes/Sources/Pcap/`
  (`PcapEngine.{h,cpp}`, `PcapModel.{h,cpp}`, `PcapWidget.{h,cpp}`),
  `DemoNodeEditorNodesObject.cpp`, `CMakeLists.txt`

## Бележка

Изискването е създадено по решение на потребителя (2026-09-06) след проучването
в `DevelopmentProcess/plans/daq-node-candidates-2026-09-05.md` (секция за
network capture). Linux-first v1 (libpcap). Windows поддръжка (WinPcap/Npcap)
се планира за v2. Unit тестове са ОТЛОЖЕНИ по стоящата инструкция
„НОВИ ТЕСТОВЕ СТОП".