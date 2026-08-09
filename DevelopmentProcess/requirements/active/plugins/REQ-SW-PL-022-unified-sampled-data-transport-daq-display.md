# REQ-SW-PL-022: Unified Sampled Data Transport & DAQ Display (Audio Output on Video Nodes)

- **Статус:** ACTIVE
- **Приоритет:** P1
- **Отговорник (роля):** Ivan (Implementation)
- **Дата:** 2026-08-09
- **Родител:** REQ-SW-PL-014
- **Зависи от:** REQ-SW-PL-013, REQ-SW-PL-020 (sibling transport pattern — референция; реална зависимост само от PL-013)

## Описание

### 1. Bug fix — тих аудио на Qt6 (коренът на цялото)

`VideoFileSourceNode` (`Sources/Video/VideoFileSourceNode.cpp:29`) и `StreamSourceNode`
(`Sources/Video/StreamSourceNode.cpp:29`) създават `QMediaPlayer` **без** `QAudioOutput`.
Qt6 изисква `setAudioOutput()` — иначе аудиото се декодира, но никога не се рутира и е
тихо. Qt5 свири по подразбиране. Поправка: `QAudioOutput` member + `setAudioOutput()`,
гарднат с `#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)`.

### 2. SampledStreamDescriptor — консолидация на 3-те припокриващи се format структури

Днес форматът на sampled stream-ове е разпокъсан в три места:

1. `GenericStreamConfig` (`BuiltInNodes/Library/types/GenericNumericTypes.h:56-68`) —
   sampleRate (int) + per-channel {name, SampleType}.
2. `QDevIOStreamConfig` (`BuiltInNodes/Library/types/QDevIOStreamConfig.h:17-28`) —
   type/sampleRate/bitsPerSample/channels/signed_/littleEndian/amplitudeScale/
   amplitudeOffset/unit/channelNames.
3. Qt Multimedia `QAudioFormat` (audio-специфичен, int kHz, без amplitude metadata).

Изискването ги слива в **един** `SampledStreamDescriptor` в
`src/plugins/common/NodeDataTypes/` с полета:

- `sampleRate` — **double** (Hz): DAQ може да е sub-Hz (0.1 Hz), аудиото е int kHz (44100.0);
- per-channel `{name, SampleType}` — 16-канален DAQ със смесени типове трябва да е
  представим;
- `SampleType` enum **разширен**: int8/uint8/int16/uint16/int24/uint24/int32/uint32/
  float32/float64 (днешният enum в `GenericNumericTypes.h:12-19` има само 6 типа);
- endianness (LE/BE);
- `unit` + `amplitudeScale` + `amplitudeOffset` (от `QDevIOStreamConfig`);
- `domain` поле (`"audio"`, `"vibration"`, `"daq"`, `"ecg"`, …) — **дискриминаторът**;
- device id / source name, first-sample timestamp (`qint64`).

Старите структури се премахват/заменят и техните ~6 консуматора се мигрират:
`AudioFrameDecoder::configure`, потребителите на `QDevIOStreamConfig`
(`GenericQDevIoConnector`, `QDevIoDisplayModel`), `GenericNumericData`,
`XYSeriesIODevice`, `GenericDisplayNode`, `AudioDisplayModel`.

### 3. SampledData NodeData тип — еволюция на GenericNumericData

`GenericNumericData` (`GenericNumericTypes.h:89-108`) вече е коректен NodeData:
QByteArray + per-channel SampleType + sampleRate + `decodeToNormalized()`. Той се
промотира/еволюира в **`SampledData`** в `src/plugins/common/NodeDataTypes/`:

- `type()` → `NodeDataType {"sample", "Sample"}`;
- header-only или минимален TU; **QtCore-only** зависимости (БЕЗ QtMultimedia coupling —
  проверено е осъществимо: единствено `AudioFrameDecoder::configure` дърпа `AudioCompat`; добавя се
  `(SampleType, bits, endian)` overload, за да остане decoder-ът QtCore-only);
- **UNIFIED decoder:** преизползва се наборът от функции на `AudioFrameDecoder` и се
  поправя латентната несъответствие — `GenericNumericTypes.cpp` дели int16 на `32768.0`
  без clamp (флоатовете са unclamped), `AudioFrameDecoder.cpp` дели на `32767.0` + clamp.
  Избира се ЕДНА конвенция (препоръчана: AudioFrameDecoder-ската — `32767` + clamp в
  [-1, 1], документирана);
- конверсията от `QAudioBuffer` става на source boundary-то — един memcpy ~7KB при
  44.1kHz stereo float32 20ms block, <1µs, пренебрежим. **Аудиото НЕ се нуждае от
  zero-copy като видео PL-020** — zero-copy е видео-специфичен проблем, не се пренася
  върху аудиото (документирано);
- `AudioData` = `SampledData` с `domain="audio"` — **без отделен клас**: един тип,
  domain-ът дискриминира.

### 4. Аудио изходен порт на video source нодовете

`VideoFileSourceNode` + `StreamSourceNode` получават аудио изход като **ПОСЛЕДЕН**
порт (Qt6: порт 2; Qt5: порт 1 — Qt5/Qt6 порт layout-ът вече се различава днес,
`VideoFileSourceNode.cpp:73-99`). Capture:

- **Qt6:** `QAudioBufferOutput` (Qt 6.8+, FFmpeg backend — проектът ползва 6.9.2),
  гарднат с `#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)`, чрез `setAudioBufferOutput`,
  сигнал `audioBufferReceived(QAudioBuffer)`;
- **Qt5:** `QAudioProbe` чрез `audioBufferProbed`.
- **ВЕРИФИКАЦИЯ по време на имплементация:** въпросът "QAudioBuffer в Qt6" даде
  противоречива информация при проучването (едното изследване каза "QAudioBuffer е
  Qt5-only", другото — "QAudioBufferOutput в Qt6 емитира QAudioBuffer"). Истината:
  `QAudioBuffer` **съществува в Qt6**, `QAudioBufferOutput` е Qt 6.8+ и емитира
  `QAudioBuffer`; `QAudioProbe` продължава да работи, но е deprecated.

Всеки буфер се обвива в `shared_ptr<SampledData>` (domain=audio) → `dataUpdated(audioPortIndex)`,
гейтнат на connection count (модел: `m_imagePortConnectionCount`, `VideoFileSourceNode.cpp:119-137`).
Празният `QAudioBuffer` при end-of-stream се флъшва/игнорира (**НЕ** се емитира EOS тип).
Threading: буферите пристигат на player-овата media нишка (или на GUI нишката тук,
понеже всичко работи на GUI нишката според CPU investigation) — в handler-а се прави
**САМО** wrap на буфера, без конверсия; QByteArray copy в handler-а е приемлив.

### 5. DAQ Display нод (DataPlot архитектура)

`AudioDisplayModel` се преименува/генерализира на **DAQ Display** (потребителят:
"не AudioDisplay, а DAQ Display или нещо по-смислено"). Нодът визуализира **всеки**
sampled source (audio, DAQ, сензори) — waveform **И** FFT през **същия** нод.

**Архитектура (дизайн на потребителя):** DataPlot нод, който хоства **серия от plot
слотове**. Всеки слот = `{channel selection, preprocessing function, plot view}`:

- **Preprocessing function** е extension point-ът: `QVector<float> (const SampledData&)`
  или подобен signature;
- v1 built-in функции: **identity** (time-domain waveform), **FFT** (magnitude spectrum);
- интерфейсът е **JIT-ready**: далечният бъдещ JIT compiler ще позволява писане на
  preprocessing функции на момента (думи на потребителя). Слот интерфейсът се проектира
  така, че scriptable/compiled функция да може да се включи по-късно (напр.
  `std::function`-базирани слотове).

**Реални charts:** днешните `GenericDisplayNode::updateTimeChart()/updateFFTChart()`
са Q_UNUSED stubs (`GenericDisplayNode.cpp:136-146`), а
`AudioDisplayModel::configureAudioView()` е no-op (`AudioDisplayModel.cpp:33-39`). DAQ
Display-ът изисква **реално** waveform + FFT рендиране. Базата е **Qt Charts**
(вече линкнато в `demo_nodeditor_nodes/CMakeLists.txt:126` и
`node_editor_ide/CMakeLists.txt:22,147`, shim `QtChartsCompat.h`) — без нови тежки
зависимости; `GenericDisplayNode::setupUi()` вече създава ChartView/Chart страници
(Time Domain + Frequency Spectrum), които да се пълнят.

**Потребление:** нодът консумира `SampledData` през `setInData` (NodeData flow), **НЕ**
QDevIO byte-stream. QDevIO mic path-ът остава както е засега (работещ, display-only;
бъдеща миграция, когато съществува unified display).

## Acceptance Criteria

- [ ] 1. **Qt6 audio fix (audible).** `VideoFileSourceNode` и `StreamSourceNode` създават
       `QAudioOutput` member + `setAudioOutput()` (`#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)`);
       при възпроизвеждане на файл/stream с аудио трак звукът се **чува и на Qt6**;
       Qt5 поведението остава непроменено.
- [ ] 2. **SampledData тип.** NodeData subclass в
       `src/plugins/common/NodeDataTypes/SampledData.{h,cpp}`, `type()` → `{"sample",
       "Sample"}`, **QtCore-only** (без QtMultimedia); носи QByteArray + per-channel
       {name, SampleType} + sampleRate + `decodeToNormalized()`; `AudioData` = SampledData
       с `domain="audio"` (без отделен клас).
- [ ] 3. **Unified decoder конвенция.** Единна конвенция за нормализация:
       signed/unsigned се делят на `(2^(bits-1) − 1)` със clamp в [-1, 1]; floats се
       clamp-ват. `GenericNumericTypes.cpp` вече не дели на `32768.0`; конвенцията е
       документирана в header-а на SampledData.
- [ ] 4. **SampledStreamDescriptor консолидация.** `GenericStreamConfig` +
       `QDevIOStreamConfig` сляти в `SampledStreamDescriptor` (всички полета от scope:
       double sampleRate, per-channel {name, SampleType}, разширен SampleType
       int8/uint8/int16/uint16/int24/uint24/int32/uint32/float32/float64, endianness,
       unit + amplitudeScale + amplitudeOffset, domain, device id/source name,
       first-sample timestamp). Старите структури са премахнати/заменени и ~6-те им
       консуматора са мигрирани (AudioFrameDecoder overload, GenericQDevIoConnector,
       QDevIoDisplayModel, GenericNumericData→SampledData, XYSeriesIODevice,
       GenericDisplayNode, AudioDisplayModel→DAQ Display).
- [ ] 5. **Аудио изходен порт.** Аудио изходът е append-нат като ПОСЛЕДЕН порт (Qt6:
       порт 2; Qt5: порт 1); каптър: Qt6 `QAudioBufferOutput` (6.8+, guard) / Qt5
       `QAudioProbe`; всеки буфер се обвива в `shared_ptr<SampledData>` (domain=audio)
       и се емитира `dataUpdated(audioPortIndex)`, гейтнат на connection count;
       празният QAudioBuffer при EOS се флъшва/игнорира (без EOS тип); в handler-а се
       прави само wrap (без конверсия).
- [ ] 6. **DAQ Display (DataPlot).** `AudioDisplayModel` е генерализиран на DAQ Display;
       показва waveform + FFT за audio през същия нод; plot слотовете са
       `std::function<QVector<float>(const SampledData&)>`-базирани (JIT-ready); v1
       built-ins: identity + FFT; рендирането е реално (QtCharts) — stubs-ите в
       `GenericDisplayNode.cpp:136-146` и `AudioDisplayModel.cpp:33-39` са заменени.
- [ ] 7. **Втори domain (доказателство за унификация).** DAQ Display-ът визуализира
       синтетичен `"vibration"` SampledData (waveform + FFT) през същия входен порт,
       без промени по нода — унифицираният тип + domain дискриминатор работят.
- [ ] 8. **Qt5 + Qt6 builds PASS + app smoke + saved-graph compat.** Приложенията
       стартират без crash; и двете версии се build-ват; съществуващата test suite
       остава зелена; старите saved graphs се зареждат без port re-indexing (аудио
       портът е append-нат последен). Unit тестовете са **отложени по решение на
       потребителя** (standing instruction — status → `DONE` чака тестовете).

## Извън обхват (бъдеща работа)

- **Mic (AudioSourceDataModel) миграция** от QDevIO към SampledData — по-късно.
- **AI аудио потребление** (AudioToTensorNode — private repo, трябва реален модел):
  window = N samples @ fixed rate, N/rate latency е присъща на windowing-а;
  `{1,C,N}` TensorData **не** носи sample rate — conversion node задава изходния rate
  като параметър.
- **DAQ/sensor device нодове** — няма хардуер още.
- **JIT preprocessing compiler** — далечно бъдеще (но slot интерфейсът го
  предвижда: std::function-базирани слотове).
- **Zero-copy аудио транспорт** — съзнателно НЕ се прави: ~7KB chunks, memcpy е µs.

## Проследимост

- **Коммити:** — (след имплементация; branch `feat/REQ-SW-PL-022-unified-sampled-data-transport-daq-display`)
- **Код:** `src/plugins/common/NodeDataTypes/SampledData.{h,cpp}`,
  `src/plugins/common/NodeDataTypes/SampledStreamDescriptor.h`,
  `src/plugins/demo_nodeditor_nodes/Sources/Video/` (VideoFileSourceNode,
  StreamSourceNode — QAudioOutput + audio порт),
  `src/plugins/demo_nodeditor_nodes/Displays/DaqDisplay/` (DAQ Display нод),
  `src/plugins/node_editor_ide/BuiltInNodes/Library/decoders/AudioFrameDecoder.{h,cpp}`
  (QtCore-only overload), `BuiltInNodes/Library/types/` (старите структури),
  `CMakeLists.txt` (и двете plugin-а)
- **Документация:** `docs/plugins/demo_nodeditor_nodes/README.md` (DAQ Display секция,
  audio port layout, decoder конвенция), traceability matrix
- **Тестове:** отложени по решение на потребителя (standing instruction). Qt5
  (5.15.2) + Qt6 (6.9.2) builds PASS; app smoke без crash; съществуващата test suite
  остава зелена.

## Бележки по имплементацията (план)

- **QAudioBuffer в Qt6 — ВЕРИФИКАЦИЯ:** изследването даде противоречива информация.
  Вярното: `QAudioBuffer` съществува в Qt6; `QAudioBufferOutput` е Qt 6.8+ и емитира
  `QAudioBuffer`; `QAudioProbe` работи, но е deprecated. Да се потвърди на Qt 6.9.2
  по време на имплементацията.
- **Design decision (документирано):** type id-ът в QtNodes е визуалната safety net, но
  потребителят избра **един unified тип** с `domain` полето като runtime дискриминатор —
  DAQ Display-ът е осцилоскоп: показването на произволен sampled сигнал е легитимно.
  QtNodes връзка между различен `type()` (напр. `"sample"` срещу нещо друго) не би
  трябвало да се случва — всички sampled източници споделят `"sample"`.
- **AudioFrameDecoder:** съществуващият `configure(const QAudioFormat&)` остава (QDevIO
  path / XYSeriesIODevice го ползва); новият `configure(SampleType, bits, endian)`
  overload е QtCore-only и се ползва от SampledData. `AudioFrameDecoder.h` днес включва
  `QtMultimedia/QAudioFormat` — overload-ът трябва да не дърпа QtMultimedia в SampledData.
- **Port layout Qt5/Qt6 се различава ОЩЕ ДНЕС** (`VideoFileSourceNode.cpp:73-99`: Qt6
  портове 0="video-frame", 1="image"; Qt5 порт 0="image"). Аудио се append-ва последен:
  Qt6 порт 2, Qt5 порт 1 — старите saved graphs запазват индексите си.
- **Threading:** в audio buffer handler-а се прави само wrap на буфера (QByteArray
  copy) — конверсията не се прави там; емисията е гейтната на connection count
  (модел `m_imagePortConnectionCount`).
- **Charts:** Qt Charts е вече свързано и за двата Qt (QtChartsCompat shim) — DAQ
  Display преизползва модела на `GenericDisplayNode::setupUi()` (ChartView + Chart
  страници), но с реално populate-ване на series-ите; няма нови зависимости.
- **QDevIOStreamConfig нюанс:** QDevIO mic path-ът остава работещ (извън обхват);
  пълното премахване на QDevIOStreamConfig може да изисква transition shim, докато
  QDevIoDisplayModel не мигрира — това не блокира консолидацията на SampledData света.

## Бележка

Изискването е създадено **преди** имплементацията (2026-08-09) по одобреното от
потребителя решение: unified SampledData транспорт + DAQ Display (DataPlot
архитектура с preprocessing slots), с domain полето като runtime дискриминатор.
Потребителят е наредил unit тестовете да се отложат за това изискване — статус →
`DONE` чака тестовете (по модела на PL-020 AC 6). Процесната клауза "branch per work
item" (AGENTS.md) важи: работата се върши на нов branch
`feat/REQ-SW-PL-022-unified-sampled-data-transport-daq-display`.
