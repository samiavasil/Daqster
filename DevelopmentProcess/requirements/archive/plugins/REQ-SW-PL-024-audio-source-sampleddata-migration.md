# REQ-SW-PL-024: AudioSource (Mic) миграция от QDevIO към SampledData (нов нод взема името, старият → _obsolete)

- **Статус:** ACTIVE
- **Приоритет:** P1
- **Отговорник (роля):** Ivan (Implementation)
- **Дата:** 2026-08-10
- **Родител:** REQ-SW-PL-014
- **Зависи от:** REQ-SW-PL-022 (SampledData, unified decoder, AudioBufferToSampled)
- **Сроден (sibling):** REQ-SW-PL-023 (DAQ Display Multi-Plot v1) — клонът на
  PL-024 се rebase-ва върху върха на PL-023 след merge (решение на потребителя,
  2026-08-10)

## Описание

Днешният mic път (`AudioSourceDataModel`) излъчва **QDevIO** byte-stream през
`AudioNodeQdevIoConnector`, каптва през `AudioWorker` + `EventThreadPull`
(`AudioSourceDataModel.cpp:87-108`) — останка от старата QDevIO архитектура.
REQ-SW-PL-022 AC 7 доказа унификацията със синтетичен "vibration" SampledData;
mic-ът остана единственият източник без SampledData. Това изискване:

### 1. Нов нод взема текущото име; старият → `_obsolete`

Потребителска директива (2026-08-10): "новите да вземат текущото име, а старите
да се ренеймнат до _obsolete", и **двете имплементации да съществуват**, за да се
бенчне дали новата дава нови възможности и поне същата скорост при равни условия.

- **Новият SampledData mic** заема името **`AudioSourceDataModel`** (клас + файлове
  `AudioSourceDataModel.{h,cpp}` + registered name `AudioSource` — както днес).
- **Старият QDevIO mic** се преименува на **`AudioSourceDataModelObsolete`**
  (клас + файлове `AudioSourceDataModelObsolete.{h,cpp}` + registered name
  `AudioSourceObsolete` + caption "AudioSource Source (obsolete)"), **със същата
  QDevIO имплементация** — заедно със своите помощници
  `AudioWorkerObsolete`, `AudioNodeQdevIoConnectorObsolete`, `EventThreadPullObsolete`
  (rename-only; grep-верифицирано: потребителите им са само mic path-ът).
- **БЕЗ alias за `AudioSource`:** старото registry име се заема от новия нод —
  старите saved graphs с "AudioSource" инстанцират **новия** SampledData нод и
  QDevIO edges-ите към/от него отпадат (type mismatch). Това е **приетото
  следствие** на "new takes the current name" — документирано.
- И двата нода са регистрирани в категория "Sources": новият "AudioSource",
  старият "AudioSource (obsolete)".

### 2. SampledData порт (новият нод)

- `AudioSourceDataModel::dataType` → `NodeDataType {"sample", "Sample"}` (от
  `SampledData().type()`); `outData` → най-новото `shared_ptr<SampledData>`
  (member `m_lastData`); `IO_connect` → стартира/спира каптурата.
- Порт layout-ът не се променя: 1 изходен порт (както днес, `nPorts` `:46-63`).
- В **новия** нод няма QDevIO типове/connector-и.

### 3. Threading — dedicated worker thread (НИКАКВА data работа на GUI нишката)

- Нов `MicCaptureWorker` QObject (`Sources/AudioSource/MicCaptureWorker.{h,cpp}`),
  **moveToThread** в модел-притежаван `QThread`.
- `QAudioSource` се създава **в worker нишката** (queued slot `start()` след
  moveToThread) — event loop-ът на QAudioSource е на worker нишката.
- Capture: `readyRead` → `QIODevice::readAll()` (PCM raw) →
  `AudioBufferToSampled::descriptorFromFormat` (`Sources/Video/AudioBufferToSampled.h:64-91`)
  + wrap в `shared_ptr<SampledData>` (едно копие — приетият boundary copy, PL-022
  §3) → сигнал `samplesReady(std::shared_ptr<SampledData>)` → **queued** към
  модела (GUI).
- Ако изходът не е свързан (`std::atomic<bool>` `m_connected`, сетнат от модела
  през queued `setCaptureEnabled`), worker-ът **дренира и не wrap-ва**.
- GUI нишката прави само: keep-latest, `dataUpdated(0)`, UI wiring. Няма mutex за
  данните — SampledData се произвежда изцяло в worker нишката и се предава по
  shared_ptr (atomic refcount); QIODevice се докосва само в worker нишката.
- Stop: queued `stop()` → `QAudioSource::stop()`; деструктор:
  `m_thread->quit(); m_thread->wait();`; worker-ът се изтрива през
  `QThread::finished` → `deleteLater`.

### 4. UI остава (споделен между двата нода)

`AudioSourceDataModelUI` (Start/Stop, device/format контроли) не се променя и не
се преименува — ползва се и от двата нода. Сигналите `Start(StartStop)` и
`ChangeAudioConnection(devInfo, format)` на новия нод се маршрутизират към
worker-а през queued слотове.

### 5. Старият QDevIO mic остава работещ

`AudioSourceDataModelObsolete` + `AudioWorkerObsolete` + `AudioNodeQdevIoConnectorObsolete`
+ `EventThreadPullObsolete` работят както днес: mic_obsolete → AudioDisplay_obsolete
(или Mux/Demux _obsolete) потокът е функционален — това е базата за бенчмарка.
Изтриването е **НАЙ-НАКРАЯ** (отделно бъдещо решение), НЕ сега.

### 6. Съвместимост (документирана последица)

Стари saved graphs с "AudioSource": инстанцират **новия** SampledData нод;
QDevIO edges-ите (към AudioDisplay/Mux/Demux) отпадат. Стари графи с
"AudioDisplay"/"MuxNode"/"DemuxNode": alias → `_obsolete` версиите, работят.
Новите графи свързват AudioSource → DAQ Display (SampledData поток).

### 7. Benchmark (старо vs ново) — част от верификацията

Ad-hoc /tmp harness (не се commit-ва): capability матрица + speed при равни
условия — старо (QDevIO `AudioSourceDataModelObsolete` path) срещу ново
(`AudioSourceDataModel` + `MicCaptureWorker`): capture throughput (buffers/sec),
end-to-end latency (timestamp в worker → timestamp в GUI слота), CPU%.
Резултатите се записват в session status файла.

## Acceptance Criteria

- [x] 1. **SampledData порт (новият нод).** Новият `AudioSourceDataModel`
       (registered name `AudioSource`) излъчва `{"sample", "Sample"}`; `outData`
       връща `shared_ptr<SampledData>`; в него **няма остатък от QDevIO**.
- [x] 2. **Dedicated worker thread.** QAudioSource живее и каптва в worker
       нишката; SampledData се предава през queued сигнал; на GUI нишката няма
       decode/PCM обработка. Проверка: qCDebug с `QThread::currentThread()`.
- [x] 3. **Старият QDevIO mic → _obsolete, работи.** `AudioSourceDataModelObsolete`
       (registered name `AudioSourceObsolete`, caption "(obsolete)") + `AudioWorkerObsolete`
       + `AudioNodeQdevIoConnectorObsolete` + `EventThreadPullObsolete` са
       преименувани със **същата имплементация**; mic_obsolete → AudioDisplay_obsolete
       потокът е функционален; **и двата нода съществуват едновременно** в
       категория "Sources". Стари графи с "AudioSource" инстанцират новия нод
       (прието следствие); стари графи с "AudioDisplay"/"MuxNode"/"DemuxNode"
       зареждат alias → `_obsolete` версиите.
- [x] 4. **Worker lifecycle.** start/stop/device-change работят през queued
       слотове; унищожаването на нода спира чисто (quit + wait, без crash,
       без закачен thread).
- [x] 5. **UI непроменен.** Start/Stop и device/format контролите на
       `AudioSourceDataModelUI` работят както днес (и за двата нода).
- [ ] 6. **Benchmark (старо vs ново mic).** Capability матрица + speed при равни
       условия (capture throughput, end-to-end latency, CPU%): QDevIO
       `AudioSourceDataModelObsolete` срещу SampledData `AudioSourceDataModel`.
       Резултатите са записани в session status файла (2026-08-10-status.md).
- [x] 7. **Qt5 + Qt6 builds PASS + app smoke.** И двете версии се build-ват;
       съществуващата test suite остава зелена; headless smoke
       (QT_QPA_PLATFORM=offscreen, ad-hoc /tmp harness): и двата mic нода
       стартират/спират; SampledData от новия стига до DAQ Display. Unit
       тестовете са **отложени по решение на потребителя** (standing
       instruction) — статус → `DONE` чака тестовете.

## Извън обхват (бъдеща работа)

- **QDevIO display свят** — `_obsolete` rename-ите на display/routing/connector
  компонентите са в **REQ-SW-PL-023**; изтриването на целия QDevIO свят е
  НАЙ-НАКРАЯ (отделно бъдещо решение).
- **Нови UI контроли** за AudioSource (списък устройства, нива) — няма.
- **AI аудио потребление** (AudioToTensorNode — private repo) — отделна работа.
- **Unit тестове** — отложени (standing instruction).

## Проследимост

- **Коммити:** `6ee9d3b` (refactor: rename old QDevIO mic + helpers to _obsolete),
  `42eb5fa` (feat: SampledData AudioSourceDataModel + MicCaptureWorker), docs —
  branch `feat/REQ-SW-PL-024-audio-source-sampleddata-migration` (rebase върху
  PL-023 след merge)
- **Код:** нови: `Sources/AudioSource/MicCaptureWorker.{h,cpp}`,
  `Sources/AudioSource/AudioSourceDataModel.{h,cpp}` (SampledData);
  rename: `AudioSourceDataModelObsolete.{h,cpp}`, `AudioWorkerObsolete.{h,cpp}`,
  `AudioNodeQdevIoConnectorObsolete.{h,cpp}`,
  `node_editor_ide/BuiltInNodes/Library/threading/EventThreadPullObsolete.{h,cpp}`;
  CMake: `demo_nodeditor_nodes/CMakeLists.txt`, `node_editor_ide/CMakeLists.txt`;
  регистрация: `demo_nodeditor_nodes/DemoNodeEditorNodesObject.cpp:46-68`
- **Документация:** `docs/plugins/demo_nodeditor_nodes/README.md` (AudioSource
  subtree, SampledData порт, threading, obsolete rename таблица, saved-graph
  consequence), traceability matrix (и двете repo-та)
- **Тестове:** отложени по решение на потребителя (standing instruction). Qt5
  (5.15.2) + Qt6 (6.9.2) builds PASS; app smoke без crash; benchmark резултати в
  session status.

## Бележки по имплементацията (план)

- **AudioCompat shim:** worker-ът ползва `AudioCompat::defaultInputDevice()` /
  `AudioCompat::preferredFormat()` (както `AudioSourceDataModel.cpp:17-18`), за да
  абстрахира Qt5 `QAudioDeviceInfo` / Qt6 `QAudioDevice` разликите. **Без**
  `#ifdef Q_OS_WIN`.
- **Metatype:** `qRegisterMetaType<std::shared_ptr<SampledData>>("std::shared_ptr<SampledData>")`
  в конструктора на модела (за queued сигнала); `qRegisterMetaType<AudioSourceDataModel::StartStop>`
  вече е там (`:14`).
- **Rename механика:** файловете на стария mic `AudioSourceDataModel.{h,cpp}` се
  `git mv`-ват на `AudioSourceDataModelObsolete.{h,cpp}` **ПРЕДИ** създаването на
  новия нод (пътищата съвпадат); после rename на класа, registered name, caption;
  после full-tree grep:
  `rg "AudioWorker|AudioNodeQdevIoConnector|EventThreadPull"` → само `_obsolete`.
- **Споделени, не се преименуват:** `AudioSourceDataModelUI.{h,cpp,ui}`,
  `AudioSourceConfig.{h,cpp,ui}`, `AudioComboModel.{h,cpp}`, `AudioCompat.h`.
- **Източник на истината за формата:** `AudioBufferToSampled` (`Sources/Video/`).
  Не се копира в AudioSource — reuse.

## Бележка

Изискването е създадено **преди** имплементацията (2026-08-10) по одобреното от
потребителя решение: нов SampledData AudioSource заема името, старият QDevIO mic
се преименува `_obsolete` и остава работещ (със своите помощници) за
benchmarking, изтриване най-накрая. Създадено като **sibling** на REQ-SW-PL-023
(PL-024 се rebase-ва върху PL-023 след merge). Процесната клауза "branch per work
item" (AGENTS.md) важи: работата се върши на нов branch
`feat/REQ-SW-PL-024-audio-source-sampleddata-migration`. Unit тестовете са
отложени по standing instruction (модел PL-022 AC 8).
