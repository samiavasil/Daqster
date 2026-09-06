# REQ-SW-PL-040: PlutoSDR RX DAQ Node (IQ streaming via libiio)

- **Статус:** ACTIVE
- **Приоритет:** High
- **Отговорник (роля):** Ivan (Implementation)
- **Дата:** 2026-09-05
- **Родител:** REQ-SW-PL-014
- **Зависи от:** REQ-SW-PL-022, REQ-SW-PL-013

## Описание

PlutoSDR RX source нод в `demo_nodeditor_nodes` plugin-а: стриймва IQ семпли от
OpenSourceSDRLab PlutoSky 7020-SDR (AD9363, pre-hacked до AD9361 режим
70 MHz–6 GHz) през libiio и ги емитира като `SampledData` (domain="iq") за
downstream обработка (DaqDisplayNode waveform/FFT, бъдещи DSP нодове).

### 1. Хардуерен контекст

PlutoSky 7020-SDR = клонинг на ADALM-Pluto върху Zynq-7020 (1GB DDR3) с AD9363BBCZ
RF трансивър, фабрично флашнат с модифициран ADI Pluto фърмуер v0.38, "хакнат" до
AD9361 режим: **70 MHz – 6 GHz, 2T2R, USB + Ethernet**. Max sample rate 61.44 MSPS
(и за AD9363), но **USB throughput-ът е ~5–7.5 MSPS** — практическият лимит за USB
gadget-а. Интеграцията е през **libiio** (C API, без Qt зависимост):

```
iio_create_context_from_uri("ip:192.168.2.1")   // USB gadget или Ethernet, същата URI
→ ad9361-phy     (честота / sample rate / gain)
→ cf-ad9361-lpc  (enable voltage0(I) + voltage1(Q))
→ iio_buffer_refill() в worker thread → IQ семпли (int16 interleaved)
```

### 2. Node архитектура (патърнът Engine → Model → Widget)

`PlutoSdrEngine → PlutoSdrModel → PlutoSdrWidget` (огледално на
`LlamaEngine → LlamaGenerateModel → LlamaGenerateWidget` в private plugin-а и на
`MicCaptureWorker → AudioSourceDataModel → AudioSourceDataModelUI` в публичния):

- **`PlutoSdrEngine`** — libiio обвивка: context create/destroy, phy атрибути
  (frequency/sample_rate/gain), buffer create/refill в worker thread, атомарен
  stop флаг + `iio_buffer_cancel()` за чисто спиране;
- **`PlutoSdrModel`** (`NodeDelegateModel`) — тънък контролер: 1 изходен порт
  (`SampledData` "sample"), гейтнат на connection count (модел
  `m_imagePortConnectionCount` от VideoFileSourceNode), обвива всеки refill-нат
  буфер в `shared_ptr<SampledData>` и емитира `dataUpdated(0)`;
- **`PlutoSdrWidget`** — UI: URI, честота (MHz), sample rate (MSPS), gain mode
  (manual/auto) + gain, Start/Stop, статус.

### 3. Data type решение: SampledData с domain="iq" (НЕ отделен IQData)

IQ семплите СА sampled data: 2 канала (I, Q), int16, interleaved — точно това,
което `SampledStreamDescriptor` описва. Избира се **един unified тип** с
`domain="iq"` като runtime дискриминатор (решението на PL-022: "DAQ Display-ът е
осцилоскоп — показването на произволен sampled сигнал е легитимно"):

- `SampledStreamDescriptor`: `sampleRate` = конфигурираният rate (Hz),
  `channels` = `[{"I", INT16}, {"Q", INT16}]`, `endianness` = LittleEndian (native),
  `unit` = "raw", `domain` = "iq", `deviceId` = "plutosdr",
  `sourceName` = "PlutoSky 7020-SDR";
- **DaqDisplayNode консумира нода без промени** — waveform на I/Q каналите и FFT
  на канал работят веднага; QtNodes връзката е по `type()` = "sample" (същият тип
  като аудио/DAQ източниците);
- отделен `IQData` тип би счупил унификацията на PL-022 и би изисквал промени по
  DaqDisplayNode — отхвърлен;
- комплексният FFT спектър (I + jQ) е бъдещо подобрение като preprocessing слот в
  DaqDisplayNode — извън обхват тук.

### 4. libiio зависимост (опционална, моделът на OpenCV)

`find_package(libiio QUIET)` (libiio доставя CMake config) с pkg-config fallback,
опционална компилация на PlutoSDR нода (моделът на `DAQSTER_USE_OPENCV` в
`demo_nodeditor_nodes/CMakeLists.txt:110-124`): без libiio plugin-ът се build-ва
нормално без нода; с libiio нодът се компилира. Това пази Qt5/Qt6 builds зелени
на машини без libiio (CI, Windows).

## Acceptance Criteria

- [ ] 1. **Регистрация.** `PlutoSdrModel` се регистрира в
       `DemoNodeEditorNodesObject::registerNodes()` като `"Daq/Sources"` —
       палитрата показва нода под "Daq/Sources".
- [ ] 2. **libiio dependency.** `find_package(libiio QUIET)` / pkg-config в
       `demo_nodeditor_nodes/CMakeLists.txt`; опционална компилация (без libiio →
       build-ът минава без нода); Qt5 И Qt6 builds PASS и в двата режима.
- [ ] 3. **Config UI.** `PlutoSdrWidget`: URI (default `ip:192.168.2.1`), честота
       (MHz, 70–6000), sample rate (MSPS), gain mode (manual/auto) + gain,
       Start/Stop бутони, статус label (connected/streaming/error).
- [ ] 4. **IQ стрийминг.** `PlutoSdrEngine` създава контекст от URI-то, конфигурира
       ad9361-phy (frequency/sample_rate/gain), enable-ва voltage0(I)+voltage1(Q)
       на cf-ad9361-lpc и чете `iio_buffer_refill()` в worker thread; IQ семплите
       (int16 interleaved) се обвиват в `SampledData` и се емитират
       `dataUpdated(0)`, гейтнато на connection count.
- [ ] 5. **SampledData domain="iq".** Дескрипторът: 2 канала `{"I", INT16}` +
       `{"Q", INT16}`, `sampleRate` = конфигурираният, `domain` = "iq",
       `deviceId` = "plutosdr"; данните са консумируеми от `DaqDisplayNode` БЕЗ
       промени по нода (waveform + FFT).
- [ ] 6. **Чисто спиране.** Stop/деструктор: атомарен stop флаг +
       `iio_buffer_cancel()` + thread join — без deadlock, без crash при
       затваряне на графа/приложението по време на стрийминг.
- [ ] 7. **Верификация.** Qt5 + Qt6 builds PASS; unit тестовете са ОТЛОЖЕНИ по
       решение на потребителя (стоящата инструкция „НОВИ ТЕСТОВЕ СТОП“ от
       2026-08-13 остава в сила — моделът на PL-022/PL-024); headless smoke
       (offscreen, без хардуер — нодът се създава, UI-то работи, липсата на
       устройство се репортва като статус, без crash); hardware smoke с
       iio_info/iio_readdev (устройството е свързано и верифицирано).

## Извън обхват (бъдеща работа)

- **TX път** — PlutoSky е 2T2R, но нодът е RX-only; TX е отделно бъдещо изискване.
- **Комплексен FFT спектър** (I + jQ) — preprocessing слот в DaqDisplayNode,
  без промяна на транспорта.
- **File record/playback, мрежови нодове** — отделни кандидати от
  `DevelopmentProcess/plans/daq-node-candidates-2026-09-05.md`.
- **libad9361-iio FIR филтри** — helper за BB rate/FIR, не е нужен за v1.

## Проследимост

- **Коммити:** —
- **Код:** `src/plugins/demo_nodeditor_nodes/Sources/PlutoSdr/`
  (`PlutoSdrEngine.{h,cpp}`, `PlutoSdrModel.{h,cpp}`, `PlutoSdrWidget.{h,cpp}`),
  `DemoNodeEditorNodesObject.cpp` (registerNodes), `CMakeLists.txt` (libiio)
- **Документация:** `docs/plugins/demo_nodeditor_nodes/README.md` (PlutoSdr секция),
  traceability matrix
- **Тестове:** отложени (стоящата инструкция „НОВИ ТЕСТОВЕ СТОП“)

## Бележка

Изискването е създадено по решение на потребителя (2026-09-05) след проучването в
`DevelopmentProcess/plans/daq-node-candidates-2026-09-05.md` (секция E — PlutoSky
7020-SDR). Ключови решения: (1) нодът живее в ПУБЛИЧНИЯ demo plugin, защото
произвежда публичния `SampledData` тип и DAQ инфраструктурата е там; (2) данните са
`SampledData` с `domain="iq"` — НЕ отделен IQData тип (унификацията на PL-022 +
консумация от DaqDisplayNode без промени); (3) libiio е опционална зависимост по
модела на OpenCV; (4) unit тестовете са ОТЛОЖЕНИ по решение на потребителя
(стоящата инструкция „НОВИ ТЕСТОВЕ СТОП“ от 2026-08-13). Gotchas от проучването:
USB throughput ~5–7.5 MSPS (не 61.44); udev rules (`53-adi-plutosdr-usb.rules`) за
non-root USB достъп; libiio v0.26 vs v1.0 API split (да се пине версия); фърмуерът
е модифициран v0.38 — валидация с `iio_info` преди имплементация; R1 няма DFU
бутон (recovery през JTAG — да не се флашва през DFU).