# REQ-SW-PL-025: DAQ Display Multi-Plot v2 (физически decode + unit оси + ring buffer)

- **Статус:** ACTIVE
- **Приоритет:** P1
- **Отговорник (роля):** Ivan (Implementation)
- **Дата:** 2026-08-11
- **Родител:** REQ-SW-PL-014
- **Зависи от:** REQ-SW-PL-023 (DAQ Display Multi-Plot v1 — базата, върху която се надгражда v2)

## Цел

Надграждане на `DaqDisplayNode` (v1, REQ-SW-PL-023) с трите функции, документирани като бъдеща работа в REQ-SW-PL-023:170-173:

1. **`decodeToPhysical()`** — физически decode: `raw × amplitudeScale + amplitudeOffset`, БЕЗ normalization, БЕЗ clamp.
2. **Unit оси** — реални мерни единици от `SampledStreamDescriptor` (време, честота, амплитуда) вместо фиксираните normalized оси.
3. **Ring buffer** — N-секундна плъзгаща се история на канал (по подразбиране 10 s), притежавана от worker нишката.

v1 показва само последния получен блок (keep-latest) в normalized [-1, 1] с фиксирани оси; v2 прави display-а **физически коректен** (реални стойности и единици) и **непрекъснат във времето** (rolling история), без да нарушава threading contract-а от v1.

## Обхват

- **В обхват:** нов физически decode път в `SampledData`; unit-етикети и data-driven Y-обхват на plot картите; worker-притежаван ring buffer с дескриптор-свързан reset; FFT от опашката на ring buffer-а; save()/restore() обратна съвместимост с v1 формата; threading contract-ът от v1 остава.
- **Извън обхват:** изтриване на `_obsolete` света, mic benchmark (PL-024), unit-test suite (standing instruction), "abs" preprocessing, JIT компилатор — виж "Извън обхват (бъдеща работа)" по-долу.

## Описание

### 1. `decodeToPhysical()` — физически decode (БЕЗ normalization, БЕЗ clamp)

Нов header-only метод в `SampledData`, огледало на `decodeToNormalizedF32` (`SampledData.h:250-273`), със същата channel layout и същия float изход (`QVector<QVector<float>>`), но **различна семантика на стойностите**:

- **Integer типове:** суровата integer стойност се чете както днес (endian-aware), но **НЕ** се дели на `(2^(bits-1) − 1)` и **НЕ** се центрира за unsigned — резултатът е `raw_int × amplitudeScale + amplitudeOffset`.
- **Float типове (FLOAT32/FLOAT64):** стойността минава **както е** (passthrough), само `× amplitudeScale + amplitudeOffset` — **без** clamp-а към [-1, 1], който днес е в `decodeF32`/`decodeF64` (`SampledData.h:111-137`).
- **Без clamp:** резултатът може да излезе извън [-1, 1] — това е целта (физически стойности, напр. ±10 V).
- `amplitudeScale`/`amplitudeOffset`/`unit` идват от `SampledStreamDescriptor` (`SampledStreamDescriptor.h:102-104`); `sampleRate` от `:98`.
- v1 пътят (`decodeToNormalizedF32`) **остава непроменен** — normalized режимът продължава да съществува (обратна съвместимост, виж §5).

### 2. Unit оси — реални единици от дескриптора

Осите на всяка карта се създават днес в `addPlotCard` (`DaqDisplayNode.cpp:422-427`) **без заглавия** и с фиксиран Y-обхват [-1, 1] (`timeRanges`, `DaqDisplayNode.cpp:133-141`, ред 140). v2:

- **Заглавия на осите (per card):**
  - Time Domain: X = `"Time (s)"`, Y = `descriptor.unit` (напр. "V", "mV", "raw").
  - Frequency Spectrum: X = `"Frequency (Hz)"`, Y = `descriptor.unit` (или "Magnitude" при normalized режим).
- **Y-обхват за физически режим:** изчислява се от **min/max на декодираните стойности** (с малък padding, ~5%), а НЕ фиксирания [-1, 1]. Това заменя твърдия `QPointF(-1.0, 1.0)` в `timeRanges` (`DaqDisplayNode.cpp:140`) само за physical картите; normalized картите запазват [-1, 1].
- **X-осите остават както са:** време в секунди от `sampleRate` (`buildTimeSeries`, `DaqDisplayNode.cpp:111`), честота в Hz до Nyquist (`buildSpectrumSeries`/`spectrumRanges`, `DaqDisplayNode.cpp:127,147`).
- `spectrumRanges` (`DaqDisplayNode.cpp:143-156`) вече смята `maxMag` от данните — остава, но за physical режим Y-обхватът е `[0, maxMag × 1.05]` в единиците на дескриптора.

### 3. Ring buffer — N-секундна плъзгаща се история (worker-притежаван)

v1 показва само последния блок (`m_lastData`, keep-latest). v2 добавя rolling история **на канал**:

- **Капацитет:** `N × sampleRate × bytesPerFrame` байта на канал (N по подразбиране = **10 s**). Пример: стерео float32 @ 44.1 kHz → 10 × 44100 × 2 × 4 ≈ 3.5 MB — приемливо.
- **Worker-притежаван:** ring buffer-ът живее **в compute task-а / worker страната** (off-GUI), НЕ в нода на GUI нишката. Всеки нов блок от `setInData` се **append-ва** към ring buffer-а на worker нишката при следващия compute pass; старите проби отпадат отпред (rolling window).
- **Descriptor-change reset:** при промяна на `sampleRate`, брой канали или `bytesPerFrame` (нов дескриптор) ring buffer-ът се **изчиства** — историята от предишния формат е невалидна.
- **FFT от опашката:** FFT входът е **най-новите** проби от опашката на ring buffer-а (tail), а не целия прозорец — спектърът винаги отразява последните ≤4096 проби (FftUtil cap, v1 §5).
- **Time Domain:** показва целия плъзгащ се прозорец (N секунди), с decimation бюджета от v1 (≤2000 точки/series).
- Ring buffer-ът е **per-channel** — всяка карта чете своя канал от историята.

### 4. Threading contract — НИКАКВА data работа на GUI нишката (запазва се от v1)

Contract-ът от REQ-SW-PL-023 §4 остава **изцяло в сила**:

- `setInData` (`DaqDisplayNode.cpp:297-322`) само запазва `shared_ptr<SampledData>` (keep-latest) и маркира dirty — **никаква обработка** на GUI нишката.
- Ring buffer append + decode (normalized ИЛИ physical) + FFT + point build + min/max обхвати — **всичко** върви в `ComputeTask`-а на node-притежавания `QThreadPool` (maxThreadCount=1, `DaqDisplayNode.cpp:44-75`).
- Резултатът (`PlotResult`) се връща queued през `DaqDisplayResultBridge` (`Qt::QueuedConnection`); `applyResult` (`DaqDisplayNode.cpp:160-174`) прави **само** `series->replace()` + `axis->setRange()`.
- Ring buffer-ът не се докосва от GUI нишката — няма mutex за данните (същият immutable-copy модел като v1).

### 5. save()/restore() — обратна съвместимост с v1 формата

v1 форматът (`DaqDisplayNode.cpp:228-246`): `{"plots": [{"title", "processing": "time"|"fft", "channel": idx}, ...]}`. v2:

- `save()` добавя **опционални** полета: node-level `"ringSeconds"` (double, default 10.0) и per-card `"mode": "normalized"|"physical"` + `"unitAxes": bool` (default true).
- `restore()` (`DaqDisplayNode.cpp:248-267`) чете новите полета с **defaults при липса**: `ringSeconds=10.0`, `mode="normalized"`, `unitAxes=true` — **старите v1 файлове се зареждат без промяна** (backward compat).
- Round-trip: v2 файл → идентични карти (title/processing/channel/mode) + ringSeconds.

## Acceptance Criteria

- [ ] 1. **`decodeToPhysical()` семантика.** Нов метод в `SampledData` (огледало на `decodeToNormalizedF32`, `SampledData.h:250-273`): integer стойностите се мащабират `raw × amplitudeScale + amplitudeOffset` **без** деление на `(2^(bits-1) − 1)` и **без** центриране; FLOAT32/FLOAT64 минават passthrough (само ×scale+offset); **няма clamp** — резултатът може да е извън [-1, 1]. Проверка: синтетичен int16 SampledData с `amplitudeScale=0.001, amplitudeOffset=0.0` → raw 32767 дава ≈32.767, а не 1.0.
- [ ] 2. **Unit оси.** Всяка карта има `QValueAxis` заглавия от дескриптора (`DaqDisplayNode.cpp:422-427`): Time Domain → X "Time (s)", Y = `descriptor.unit`; Frequency → X "Frequency (Hz)", Y = unit/Magnitude. Physical картите имат Y-обхват от **min/max на данните** (с padding), а не фиксирания [-1, 1] (`timeRanges`, `DaqDisplayNode.cpp:140`); normalized картите запазват [-1, 1].
- [ ] 3. **Ring buffer (rolling история).** Всеки канал държи N-секундна история (капацитет `N × sampleRate × bytesPerFrame`, default **10 s**); новите блокове се append-ват, старите отпадат; Time Domain показва целия прозорец (≤2000 точки/series, v1 бюджет).
- [ ] 4. **Descriptor-change reset.** При промяна на `sampleRate`/брой канали/`bytesPerFrame` ring buffer-ът се изчиства и започва отново — няма смесване на проби от различни формати.
- [ ] 5. **FFT от опашката.** Спектърът се изчислява от **най-новите** проби на ring buffer-а (tail, ≤4096 входа през FftUtil), а не от целия прозорец.
- [ ] 6. **Threading contract.** Ring buffer append + decode (normalized/physical) + FFT + point build + min/max — **всичко** на worker нишката; на GUI нишката само `series->replace()` + `axis->setRange()`. Проверка: qCDebug с `QThread::currentThread()` в task-а и в repaint-а (както v1 AC 4).
- [ ] 7. **save()/restore() backward compat.** v1 файл (без новите полета) се зарежда с defaults (`ringSeconds=10`, `mode="normalized"`, `unitAxes=true`); v2 файл round-trip-ва идентични карти + ringSeconds.
- [ ] 8. **Qt5 + Qt6 builds PASS + ctest green + smoke.** И двете версии се build-ват; съществуващата test suite остава зелена; headless smoke (offscreen, ad-hoc /tmp harness) със синтетичен 2-канален SampledData (physical режим, unit оси, ring buffer) показва time + FFT карти без crash. Unit тестовете са **отложени по решение на потребителя** (standing instruction) — статус → `DONE` чака тестовете.

## Извън обхват (бъдеща работа)

- **Изтриване на QDevIO display света (`_obsolete` компонентите)** — НАЙ-НАКРАЯ, отделно бъдещо решение (наследено от PL-023 §7); v2 не пипа obsolete кода.
- **Mic (AudioSourceDataModel) benchmark** — отделно изискване REQ-SW-PL-024.
- **Unit-test suite** — отложена по standing instruction (модел PL-023 AC 10).
- **"abs" preprocessing** — съзнателно НЕ се добавя (решение на потребителя, PL-023).
- **JIT preprocessing compiler** — далечно бъдеще (slot интерфейсът остава `std::function`-базиран).
- **UI за конфигуриране на ring buffer-а от потребителя** — v2 фиксира N по подразбиране (10 s) и го пази в save()/restore(); интерактивен контрол за N е бъдеща работа.

## Верификация

1. **Builds:** Qt5 (5.15.2) + Qt6 (6.9.2) — `./scripts/build.sh qt5` / `qt6` PASS (публично repo).
2. **ctest:** съществуващата suite остава зелена и на двете версии (requirements_manager_tests, matrix, exporter, gui, demo_nodeditor_nodes_tests).
3. **Headless smoke:** offscreen (`QT_QPA_PLATFORM=offscreen`, ad-hoc /tmp harness, не се commit-ва) със синтетичен 2-канален SampledData: physical decode (проверка на стойностите извън [-1, 1]), unit axis заглавия, ring buffer rolling прозорец, FFT от опашката — без crash.
4. **Benchmark (v1 vs v2):** ad-hoc /tmp harness — capability матрица + speed при равни условия: v1 (normalized, keep-latest) срещу v2 (physical + ring buffer): refresh rate (QElapsedTimer за N обновявания) и CPU%. Резултатите се записват в session status файла.

## Проследимост

- **Коммити:** попълва се по време на имплементацията — branch `feat/REQ-SW-PL-025-daq-display-multiplot-v2`
- **Код:** `src/plugins/common/NodeDataTypes/SampledData.h` (`decodeToPhysical`, огледало на `decodeToNormalizedF32` :250-273), `src/plugins/common/NodeDataTypes/SampledStreamDescriptor.h` (unit :102, amplitudeScale :103, amplitudeOffset :104), `src/plugins/demo_nodeditor_nodes/Displays/DaqDisplay/DaqDisplayNode.{h,cpp}` (axes :422-427, timeRanges :133-141, spectrumRanges :143-156, save :228-246, restore :248-267, ComputeTask :44-75, onRefreshTick :601-619)
- **Документация:** `docs/plugins/demo_nodeditor_nodes/README.md` (DAQ Display секция — v2: physical decode, unit axes, ring buffer), traceability matrix (и двете repo-та)
- **Тестове:** отложени по решение на потребителя (standing instruction). Qt5 (5.15.2) + Qt6 (6.9.2) builds PASS; ctest green; smoke без crash; benchmark резултати в session status.

## Бележки по имплементацията (план)

- **decodeToPhysical:** header-only, копира структурата на `decodeToNormalizedF32` (`SampledData.h:250-273`), но извиква нови raw decoders (без `clampUnit`, без normalization); float passthrough без clamp-а от `decodeF32`/`decodeF64` (`SampledData.h:111-137`). `amplitudeScale`/`offset` се четат от `m_descriptor` веднъж на повикване.
- **Ring buffer:** `QVector<float>` per channel (след decode) или raw `QByteArray` per channel (преди decode)? План: **raw append** (bytes) в worker task-а, decode при draw — запазва се едно копие и се позволява FFT от tail без повторен decode на целия прозорец. Капацитетът се изчислява от дескриптора при първия блок и се проверява при всеки следващ (descriptor-change reset, AC 4).
- **CardConfig:** добавя се `mode` (normalized/physical) + `unitAxes` — snapshot за worker-а (`DaqDisplayNode.h:154-157`); `PlotCard` (`DaqDisplayNode.h:133-151`) получава axis title state.
- **Y-обхват:** physical time-domain → min/max от декодираните стойности с ~5% padding; normalized → [-1, 1] както днес (`timeRanges`, `DaqDisplayNode.cpp:140`).
- **save()/restore():** новите полета са опционални и се четат с defaults — v1 файловете минават без промяна (AC 7).
- **Threading:** ring buffer-ът се append-ва само в `ComputeTask::run` (`DaqDisplayNode.cpp:56-69`); GUI нишката не го докосва — същият immutable-copy модел като v1, без mutex за данните.

## Бележка

Изискването е създадено **преди** имплементацията (2026-08-11) като RDD gate за Phase 6 v2, по одобреното от потребителя решение: физически decode (`raw × amplitudeScale + amplitudeOffset`, без normalization/clamp), реални unit оси от `SampledStreamDescriptor` и worker-притежаван N-секунден ring buffer (default 10 s) — трите функции, документирани като бъдеща работа в REQ-SW-PL-023:170-173. Процесната клауза "branch per work item" (AGENTS.md) важи: работата се върши на нов branch `feat/REQ-SW-PL-025-daq-display-multiplot-v2` от върха на PL-023 (develop). Unit тестовете са отложени по standing instruction (модел PL-023 AC 10).