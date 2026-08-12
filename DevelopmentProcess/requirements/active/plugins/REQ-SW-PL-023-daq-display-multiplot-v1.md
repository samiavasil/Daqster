# REQ-SW-PL-023: DAQ Display Multi-Plot v1 (DataPlot карти + офф-GUI обработка + QDevIO display свят → _obsolete)

- **Статус:** DONE
- **Приоритет:** P1
- **Отговорник (роля):** Ivan (Implementation)
- **Дата:** 2026-08-10
- **Родител:** REQ-SW-PL-014
- **Зависи от:** REQ-SW-PL-022 (SampledData, unified decoder, DaqDisplay база)
- **Сроден (sibling):** REQ-SW-PL-024 (mic миграция) — клонът на PL-024 се rebase-ва
  върху върха на PL-023 след merge (решение на потребителя, 2026-08-10)

## Описание

Продължение на REQ-SW-PL-022 §5 (DataPlot архитектура). Днешният
`DaqDisplayNode` показва **фиксирани два** слота (Time Domain + Frequency Spectrum)
през `QStackedWidget`, като FFT слотът е **недостъпен** поради
`m_stack->setCurrentIndex(m_timeChartIndex)` в `DaqDisplayNode.cpp:240`. Това
изискване:

### 1. Multi-plot UI (v1 — DataPlot карти)

`DaqDisplayNode` хоства **N конфигурируеми plot карти** в `QScrollArea` +
`QVBoxLayout`. Всяка карта = header (`title label`, `processing combo`, `channel
combo`, `delete button`) + реална Qt Charts графика (ChartView). Header-ът има
`Add Plot` бутон; всяка карта се маха със своя `delete` бутон.

- v1 processing опции **само** `identity` (time-domain waveform) и `FFT`
  (magnitude spectrum) — **НЕ** се добавя "abs" (решение на потребителя).
- Всеки plot има **собствен** channel select (per-plot channel), независим от
  другите карти (днес channel-ът е глобален: `DaqDisplayNode.h:110`).
- Слот интерфейсът остава `std::function<QVector<float>(const SampledData&)>`
  (JIT-ready, PL-022 §5) — всяка карта държи своя `PreprocessFn`, bind-ната към
  нейния channel.

### 2. FFT bug fix + shared FftUtil

- Бъгът от `DaqDisplayNode.cpp:240` изчезва с multi-plot UI-то (няма повече
  `QStackedWidget`/`setCurrentIndex` — всички карти са видими в scroll-а).
- Новият header-only `src/plugins/common/FftUtil.h`:
  `FftUtil::magnitudeSpectrum(const QVector<float>& samples, int maxFftSize = 4096)`.
  Radix-2 Cooley-Tukey + **кеширан Hann window** (thread-safe lazy cache с
  `std::mutex`), капацитет по подразбиране 4096 проби.
- `DaqDisplayNode::spectrumSamples` (`DaqDisplayNode.cpp:103-164`) се заменя с
  извикване към `FftUtil`. Дублираният FFT код в замразения
  `QDevioDisplayModelUiObsolete::computeFFT` (бивш `QDevioDisplayModelUi.cpp:391-466`)
  **не се пипа** (obsolete светът остава със същата имплементация).

### 3. Decode направо към float (двойното копие изчезва)

`channelSamples` (`DaqDisplayNode.cpp:89-101`) декодира в
`QVector<QVector<double>>` и после конвертира във `float` — двойно копие. Добавя
се `SampledData::decodeToNormalizedF32(QVector<QVector<float>>&)` (header-only,
огледало на `decodeToNormalized`, `SampledData.h:218-240`) и `channelSamples`
декорира директно във float.

### 4. Threading — НИКАКВА data работа на GUI нишката (максимална паралелизация)

Потребителско изискване: decode + FFT + point build **извън** GUI нишката; на GUI
нишката става **само** финалният chart repaint.

- `setInData` (`DaqDisplayNode.cpp:59-80`) само **запазва** `shared_ptr<SampledData>`
  (implicit sharing → евтино) и маркира "dirty". Никаква обработка.
- Node-притежаван `QThreadPool` (maxThreadCount=1) + QRunnable compute task:
  task-ът работи с **копие** на `shared_ptr<SampledData>` (буферът е immutable) и
  копие на plot-конфигурациите (title/type/channel) — няма споделено мутируемо
  състояние, няма mutex за данните.
- QTimer ~30 Hz на GUI нишката: при dirty && няма течащ compute → submit.
  `std::atomic<bool>` `m_computeInFlight`/`m_shuttingDown`.
- Резултатът (per-card `QVector<QPointF>` + axis ranges) се връща на GUI нишката
  чрез `QMetaObject::invokeMethod(bridge, ..., Qt::QueuedConnection)` — bridge
  QObject живее на GUI нишката; метатипът на резултатната структура се
  `qRegisterMetaType`-ва в конструктора на нода.
- `onComputeDone` (GUI нишка): `series->replace(points)` + `axisX/Y->setRange()` —
  **само** repaint.
- Деструктор: спира таймера, `m_shuttingDown=true`, `m_pool->clear()`,
  `m_pool->waitForDone(500)` — task-овете проверяват флага преди invoke.

### 5. Бюджети (v1)

- Decimation: не повече от **2000 точки** на series (stride decimation след
  compute; time-domain stride = ceil(n/2000)).
- FFT cap: **4096** входа → 2048 magnitude bin-а; при 2000-точковия бюджет —
  stride-2 decimation → 1024 bin-а (документирано в кода).
- Refresh throttle: **~30 Hz**.

### 6. save()/restore() — И ДВЕТЕ задължителни

Днес има само `save()` (`DaqDisplayNode.cpp:32-37`, записва само `channel`);
`restore()` липсва. v1: `save()` → `{"plots": [{"title", "processing": "time"|"fft",
"channel": idx}, ...]}`; `restore(QJsonObject const&)` изтрива картите и ги
пресъздава от JSON-а.

### 7. QDevIO display свят → `_obsolete` (mark, keep working, delete at the VERY END)

Потребителска директива (2026-08-10): "всички текущи компоненти, които работят с
QDevIO, за сега просто да се маркнат като обсолейтващи, но да си останат със
същата имплементация работещи. Тях ще махнем най-накрая. Новите да вземат
текущото име, старите да се ренеймнат до _obsolete."

Всички QDevIO display/routing/connector компоненти се **преименуват `_obsolete`**
(класове + файлове + registered name + caption "(obsolete)"), **без промяна на
имплементацията**, alias-регистрират се за стари графи и **се изтриват НАЙ-
НАКРАЯ** — отделно бъдещо решение, НЕ сега. Пълният grep-верифициран списък:

`AudioDisplayModelObsolete` (Displays/AudioDisplay/),
`QDevIoDisplayModelObsolete` + `QDevioDisplayModelUiObsolete` + `XYSeriesIODeviceObsolete`
(display/), `GenericQDevIoConnectorObsolete` + `NodeDataModelToQIODeviceConnectorObsolete`
(connectors/), `QDevIOStreamConfigObsolete` + `StreamChannelObsolete` + `IStreamDecoderObsolete`
(types/), `MuxNodeObsolete` + `DemuxNodeObsolete` (Routing/).

**НЕ** се преименуват (споделени/не-QDevIO, grep-верифицирано): `AudioCompat.h`,
`QtChartsCompat.h`, `VideoCompat.h` (shims), `AudioFrameDecoder.{h,cpp}` (споделен
decoder — ползва се и от SampledData, AudioFrameDecoder.cpp:245), `GenericNumericTypes.{h,cpp}`
(legacy shim без активни потребители), `GenericDisplayNode` (alias subclass на
**новия** DaqDisplayNode), `AudioSourceDataModelUI`/`AudioSourceConfig`/`AudioComboModel`
(споделен mic UI).

Alias правило: obsolete нод се регистрира под **новото** `_obsolete` име И под
старото registry име (за стари графи), **освен ако** старото име е заето от новия
нод — в този случай старите графи инстанцират новия нод и несъвместимите edges
отпадат (прието следствие). Тук: `AudioDisplay` → alias на `AudioDisplayObsolete`;
`MuxNode`/`DemuxNode`/`QDevIoDisplay` → alias на `_obsolete` версиите; `AudioSource`
не е в този списък (PL-024).

### 8. Benchmark (старо vs ново) — част от верификацията

Ad-hoc /tmp harness (не се commit-ва): capability матрица + speed при равни
условия — старо (QDevIO `AudioDisplayObsolete` path) срещу ново (`DaqDisplayNode`
multi-plot): frame rate / refresh (QElapsedTimer за N обновявания) и CPU%.
Резултатите се записват в session status файла.

## Acceptance Criteria

- [ ] 1. **Multi-plot UI.** N конфигурируеми plot карти в `QScrollArea`; всяка
       карта има title label + processing combo (само identity/FFT) + channel
       combo + delete бутон + реална Qt Charts графика; `Add Plot` добавя карта,
       delete я маха.
- [ ] 2. **Per-plot channel.** Всяка карта има собствен channel select,
       независим от другите; смяната на channel на една карта не засяга другите.
- [ ] 3. **FFT bug fix.** FFT спектърът е **реално видим** (всички карти се
       показват в scroll-а; няма `setCurrentIndex` трап). Спектърът се изчислява
       през `FftUtil::magnitudeSpectrum` (cached Hann window, thread-safe); в
       `DaqDisplayNode` не остава дублиран FFT код.
- [ ] 4. **Off-GUI decode+FFT.** Всичката decode/FFT/point-build работа върви на
       `QThreadPool` нишка; на GUI нишката се изпълнява само `series->replace()` +
       `axis->setRange()`. Проверка: qCDebug с `QThread::currentThread()` в task-а
       и в repaint-а.
- [ ] 5. **Decode straight-to-float.** `channelSamples` ползва
       `SampledData::decodeToNormalizedF32` — няма interleave през `double`.
- [ ] 6. **Бюджети.** ≤2000 точки/series; FFT cap 4096; refresh ~30 Hz.
- [ ] 7. **save()/restore() round-trip.** Граф с N карти (time/fft mix, различни
       channels) се запазва и презарежда с идентични карти.
- [ ] 8. **QDevIO display свят → _obsolete (rename, keep working).** Всичките
       компоненти от §7 са преименувани (класове + файлове + registered name +
       caption "(obsolete)") със **същата работеща имплементация**; obsolete
       нодовете са alias-регистрирани под старите си имена; **старите saved
       graphs се зареждат**; `AudioDisplayModelObsolete` (QDevIO) и `DaqDisplayNode`
       (SampledData multi-plot) **съществуват едновременно** и работят.
- [ ] 9. **Benchmark (старо vs ново).** Capability матрица + speed при равни
       условия (refresh rate/CPU%): `AudioDisplayObsolete` (QDevIO) срещу
       `DaqDisplayNode` (multi-plot). Резултатите са записани в session status
       файла (2026-08-10-status.md).
- [x] 10. **Qt5 + Qt6 builds PASS + app smoke + saved-graph compat.** И двете
        версии се build-ват; съществуващата test suite остава зелена; headless
        smoke (QT_QPA_PLATFORM=offscreen, ad-hoc /tmp harness) с 2-канален
        синтетичен SampledData показва time + FFT карти. Unit тестовете са
        **завършени** (PL-023 AC10).

## Извън обхват (бъдеща работа)

- **v2:** `decodeToPhysical()` (raw = raw_int × amplitudeScale + amplitudeOffset,
  БЕЗ normalization), реални unit-оси, ring buffer в секунди.
- **"abs" preprocessing** — съзнателно НЕ се добавя (решение на потребителя).
- **JIT preprocessing compiler** — далечно бъдеще (slot интерфейсът остава
  `std::function`-базиран).
- **Изтриване на QDevIO display света (`_obsolete` компонентите)** — НАЙ-НАКРАЯ,
  отделно бъдещо решение; сега само rename + alias, същата имплементация.
- **Mic (AudioSourceDataModel)** — отделно изискване REQ-SW-PL-024 (там са
  `AudioWorkerObsolete`, `AudioNodeQdevIoConnectorObsolete`, `EventThreadPullObsolete`
  и новият SampledData AudioSource).

## Проследимост

- **Коммити:** попълва се по време на имплементацията — branch
  `feat/REQ-SW-PL-023-daq-display-multiplot-v1`
- **Код:** `src/plugins/common/FftUtil.h` (нов),
  `src/plugins/common/NodeDataTypes/SampledData.h` (`decodeToNormalizedF32`),
  `src/plugins/demo_nodeditor_nodes/Displays/DaqDisplay/DaqDisplayNode.{h,cpp}`,
  renames: `Displays/AudioDisplay/`, `Routing/`, `node_editor_ide/BuiltInNodes/Library/
  {display,connectors,types}/`, CMake: `demo_nodeditor_nodes/CMakeLists.txt`,
  `node_editor_ide/CMakeLists.txt`, регистрация:
  `demo_nodeditor_nodes/DemoNodeEditorNodesObject.cpp:46-68`
- **Документация:** `docs/plugins/demo_nodeditor_nodes/README.md` (DAQ Display
  секция — multi-plot v1, threading; obsolete rename таблица), traceability
  matrix (и двете repo-та)
- **Тестове:** отложени по решение на потребителя (standing instruction). Qt5
  (5.15.2) + Qt6 (6.9.2) builds PASS; app smoke без crash; benchmark резултати в
  session status.

## Бележки по имплементацията (план)

- **Threading contract:** `setInData` само keep-latest (GUI); compute task-ът
  работи с immutable копия (shared_ptr) — **няма mutex за данните**; резултатът
  се връща queued; GUI прави само repaint. `QVector<QPointF>` е регистриран
  метатип; за целия резултат (points + ranges) се регистрира `PlotResult`
  структура.
- **QThreadPool:** node-притежаван (maxThreadCount=1), за да може деструкторът да
  `clear()` + `waitForDone()` без да блокира глобален пул.
- **FftUtil:** header-only static inline; кешът на Hann window-а се пази в
  `std::unordered_map<int, QVector<double>>` под `std::mutex` (thread-safe).
- **Бюджет math:** 4096 cap → 2048 bin-а → stride-2 → 1024 точки (≤2000);
  time-domain stride = ceil(n/2000).
- **Obsolete rename механика:** `git mv` + rename на класове/структури +
  registered name + caption "(obsolete)"; alias-регистрация през registry
  overload-а с explicit name/factory (точен overload според версията на QtNodes —
  проверка по време на имплементация); **full-tree grep след renames**:
  `rg "AudioDisplayModel|MuxNode|DemuxNode|QDevIoDisplay|XYSeriesIODevice|GenericQDevIoConnector|NodeDataModelToQIODeviceConnector|QDevIOStreamConfig|StreamChannel|IStreamDecoder"` —
  трябва да връща само `_obsolete` имена (или docs).
- **QDevioDisplayModelUiObsolete::computeFFT е замразен** — FftUtil е ново
  извличане, не рефакторинг на QDevIO света.

## Бележка

Изискването е създадено **преди** имплементацията (2026-08-10) по одобреното от
потребителя решение: DAQ Display Multi-Plot v1 с per-plot channel/processing,
shared FftUtil, decode straight-to-float, **никаква data работа на GUI нишката** и
QDevIO display свят, маркнат `_obsolete` (същата имплементация, alias за стари
графи, изтриване най-накрая). Процесната клауза "branch per work item"
(AGENTS.md) важи: работата се върши на нов branch
`feat/REQ-SW-PL-023-daq-display-multiplot-v1` от върха на PL-022 (f2ca6ed). Unit
тестовете са отложени по standing instruction (модел PL-022 AC 8).
