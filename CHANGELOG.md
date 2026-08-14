# Changelog
Всички значими промени по този проект ще бъдат документирани в този файл.

Форматът следва [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) и
проектът използва [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- **Video нодове** (`src/plugins/demo_nodeditor_nodes/Sources/Video/`):
  - `VideoCompat.h` — Qt5/Qt6 multimedia абстракция (QVideoProbe ↔ QVideoSink, camera enumeration, media source assignment, playback-state сигнали)
  - `CameraSourceNode` — заснемане от QCamera (default или избран device)
  - `VideoFileSourceNode` — възпроизвеждане на локален видео файл (QMediaPlayer)
  - `StreamSourceNode` — възпроизвеждане на HTTP/RTSP stream (QMediaPlayer)
  - `VideoOutputNode` — live preview на incoming кадри (QLabel, pass-through)
  - `VideoModifierNode` → `VideoTransformNode` (REQ-SW-PL-019) — конфигурируем transform node: 8 базови операции (RGB Channel Swap, Grayscale, Invert, Brightness, Contrast, Blur, Flip, Sepia) + опционални OpenCV операции (GaussianBlur, Canny, Threshold) при открит OpenCV
  - Регистрирани под категория "Video" в demo node editor plugin
- **Requirements Manager plugin** (`src/plugins/requirements_manager/`):
  - Standalone application plugin с requirements дърво (REQ-SW-002..008)
  - Навигация: auto-clear filter при линк клик + back/forward история
  - Search engine (REQ-SW-PL-011)
  - Multi-repository requirements view — merge на public/private requirements (REQ-SW-PL-012)
- **Dependency graph viewer** (REQ-SW-009) — интерактивен граф на зависимостите между requirements
- **Traceability matrix** (REQ-SW-010) — изглед и export на traceability матрицата
- **Typed requirement IDs** — поддръжка на `REQ-SW-<TYPE>-NN` схема с формат валидация (с миграция на съществуващите ID-та)
- **Sugiyama auto-layout** (REQ-SW-PL-016) — автоматично подреждане на dependency graph
- **Phase status indicators** (REQ-SW-PL-017) — индикатори за фаза/статус в requirements main view
- **Plugin security & vendor trust store** (REQ-SW-FW-007) — ново изискване за plugin сигурност
- **Unit тестове за REQ-SW-PL-011/012/017/018/019** (2026-08-07, Qt5 5.15.2 + Qt6 6.9.2 PASS):
  - requirements manager shared binary `requirements_manager_tests` **87/87** (TestSearchEngine 19, TestMerge 6, validator +4, TestParser +5 phaseStatus) + `requirements_manager_matrix_tests` 7/7 + `requirements_manager_exporter_tests` 7/7
  - video `demo_nodeditor_nodes_tests` **25/25** (16 VideoTransformOps + 9 StreamUrlValidator)
  - комити: `825a9b4` (PL-017), `dff234a` (PL-011), `0429ea8` (PL-012), `3048fbd` (refactor: StreamUrlValidator), `0cf6d19` (fix: Qt5 vertical flip), `bac4503` (video suite), `9d90fff` (fix: exporter CSV header)
- **ArithmeticLogic нод** (`BuiltInNodes/Operators/ArithmeticLogic/`):
  - `ExprParser` — recursive descent C++ expression evaluator (всички C/C++ оператори: `+`,`-`,`*`,`/`,`%`,`&`,`|`,`^`,`~`,`<<`,`>>`,`&&`,`||`,`!`,`==`,`!=`,`<`,`>`,`<=`,`>=`,`?:`)
  - `ArithmeticLogicModel` — конфигурируем нод: тип (int/double), 2–8 входа, expression field, optional strobe
  - Променливи `a`–`h` отговарят на входните портове
- **NodeEditorWidget Shared Component** (`src/plugins/node_editor_widget/`):
  - `NodeEditorWidget` - споделен Qt Widgets GUI за node-based редактори
  - `ChatGraphModel` - loop-enabled графичен модел
  - `node_editor_widget_global.h` - export macro (`NODE_EDITOR_WIDGET_EXPORT`)
  - Автоматична инжекция на стандартните Daqster ноди (Audio, Media, Graphs, AI, etc.)
- **NodeEditorApp Plugin** (`src/plugins/node_editor_app/`):
  - Базов графичен плъгин използващ `NodeEditorWidget`
  - `APPLICATION_PLUGIN` тип с `create_plugin()` макро
- **ChatGraphModel в споделената библиотека** — преместен от `node_editor_ide/` в `BuiltInNodes/Library/types/` за generality
- **REQ-SW-PL-020** (zero-copy видео дисплей):
  - `VideoFrameData` — zero-copy споделен data type (QVideoFrame Qt6 / QImage Qt5)
  - Dual-output source nodes (Qt6): `CameraSourceNode` и `VideoFileSourceNode` изпращат `VideoFrameData` (zero-copy) + `ImageData` (legacy)
  - `VideoOutputNode` — Qt6 GPU дисплей чрез detached `QVideoWidget`; Qt5 fallback към `QLabel` pixmap
  - `VideoCompat` — Qt5/Qt6 мултиплекс abstraction helpers (presentFrameCompat, presentImageCompat, connectPlayerError, connectCameraError, variantToInt, QOverload)
  - Windows cross-platform compliance (QStandardPaths, без Linux-only пътища)
  - Комити: `b5c9651` (req), `157f34d` (VideoFrameData+shim), `085f63d` (VideoOutputNode), `0f92a9c` (CMake), `c873b43` (docs), `9d0f178` (AC/status)
  - Статус: ACTIVE (impl готов, unit тестове отложени)
- (REQ-SW-PL-020) VideoFrameData и VideoOutputNode unit тестове (PL-020 AC6) — e054396.
- **REQ-SW-PL-027** (Video Pipeline Instrumentation & Overlay — първи консуматор на REQ-SW-FW-008):
  - Domain getters: `Daqster::Perf::Domain::{avg,min,max,count}(stage)` (read-only, -1 при липса на samples) в `PerfProfiler.{h,cpp}`
  - Source инструментиране: `StreamSourceNode`, `VideoFileSourceNode`, `CameraSourceNode` записват inter-frame gap (`"source.frame_interval"`) + wrap/emit сегмент (`"source.wrap_emit"`) и latch-ват `handleType`/`pixelFormat` (HW/SW маркери) в домейн `"video"`
  - Output инструментиране: `VideoOutputNode` записва `"output.present"` (blit) + `"output.total"` и latch-ва маркерите от incoming кадър
  - Qt6-only Overlay + чекбокс: „Perf" чекбокс → `Domain::get("video").setEnabled()` + 500 ms `QTimer` + полупрозрачен `QLabel` бейдж (child на detached `QVideoWidget`, горе-ляво): `HW|SW · fmt=… · handle=… · fps=… · gap=…ms · present=…ms · total=…ms`
  - `VideoPerfBadge.{h,cpp}` — pure QtCore-only `formatPerfBadge()` форматър (без widgets/QtMultimedia) + детерминистичен unit тест (8 slots)
  - Премахнат временният `VideoDiag` диагностичен блок; нулев разход при `DAQSTER_ENABLE_PERF=OFF`
  - Комити: `885ad11`, `5c0c831`, `7682bb1`, `3ecd5c0`
- **REQ-SW-PL-027 (периодичен конзолен ред + self-CPU)** — copy-paste-ващи перф числа на Qt5 + Qt6:
  - `ProcessCpu` (`src/frame_work/base/src/perf/ProcessCpu.{h,cpp}`) — self-CPU семплър за текущия процес (Linux `/proc/self/stat` utime+stime в clock ticks / Windows `GetProcessTimes()` KernelTime+UserTime); първият sample задава базовата линия и връща 0.0
  - Периодичен 5 s конзолен лог в `VideoOutputNode` (Qt5 + Qt6): `[PERF] video | SW | fmt=NV12 | handle=NoHandle | fps=30 | gap=…ms | present=…ms | total=…ms | cpu=…%` през `qCDebug(lcPerf)` (стабилен `[PERF] video` префикс за grep/copy-paste); чекбоксът „Perf" вече е и на двете Qt версии
  - Qt5 инструментиране на display пътя (`"output.present"` около `updateDisplay()`, `"output.total"` около целия image клон) + Qt5 инструментиране на source нодовете (`"source.frame_interval"` gap + `"source.wrap_emit"` около QVideoFrame→QImage конверсията)
  - `formatPerfLine()` pure QtCore-only форматър в `VideoPerfBadge.{h,cpp}` + детерминистични unit тестове (5 нови slot-а, общо 15 в `TestVideoPerfBadge`)
  - Комити: `0eb005b`, `7a71e12`, `fb0455d`, `952e416`
- **REQ-SW-PL-022** (unified sampled data transport & DAQ Display):
  - `SampledData` — единен NodeData тип (`{"sample","Sample"}`) в `src/plugins/common/NodeDataTypes/`, QtCore-only, с QByteArray + per-channel `{name, SampleType}` + double sampleRate + `decodeToNormalized()`; `AudioData` = SampledData с `domain="audio"` (без отделен клас)
  - `SampledStreamDescriptor` — консолидация на `GenericStreamConfig` + `QDevIOStreamConfig`: разширен `SampleType` enum (int8/uint8/int16/uint16/int24/uint24/int32/uint32/float32/float64), endianness, unit + amplitudeScale + amplitudeOffset, `domain` поле ("audio"/"vibration"/"daq"/"ecg"/…), device id/source name, first-sample timestamp
  - Unified decoder конвенция — signed/unsigned делят на `(2^(bits-1) − 1)` със clamp в [-1,1], floats се clamp-ват (fix на старата GenericNumericTypes 32768/no-clamp конвенция); `AudioFrameDecoder` получи QtCore-only `configure(SampleType, bits, endian)` overload
  - SampledData аудио изходен порт на `VideoFileSourceNode` + `StreamSourceNode` (append-нат ПОСЪДЪН — Qt6 порт 2, Qt5 порт 1 — старите saved graphs запазват индексите); capture: Qt6 `QAudioBufferOutput` (6.8+) / Qt5 `QAudioProbe`; буферите се обвиват в `shared_ptr<SampledData>` и се емитират гейтнати на connection count
  - **DAQ Display нод** — `Displays/DaqDisplay/DaqDisplayNode.{h,cpp}`: реален Qt Charts waveform + FFT за всякarn плъгин с sampled данни (audio/DAQ/sензори); plot слотове `std::function<QVector<float>(const SampledData&)>` (JIT-ready), v1 built-ins identity + FFT; `GenericDisplayNode` е legacy alias, `AudioDisplayModel::configureAudioView` no-op заменен с реален UI config
  - Комити: `3929326` (req), `12076a2` (Qt6 audio fix), `cf5d0ae` (SampledData+descriptor), `6395220` (decoder convention), `75e291c` (audio port), `c5559b0` (DAQ Display), `8c56e83` (Qt5/Qt6 build fixes)
  - Статус: ACTIVE (impl готов, unit тестове отложени)
- **REQ-SW-PL-023** (DaqDisplayNode Multi-Plot v1):
  - `DaqDisplayNode` — плъгин за мултиплейт plotting чрез Qt Charts, FFT за всякarn sampled источник (audio/DAQ/sензори), per-plot channel/type, off-GUI compute via `QThreadPool`, save/restore via `QDataStream`
  - `FftUtil` — утилитарна библиотека за FFT-раундлайн, `magnitudeSpectrum`, `decodeToNormalizedF32`
  - Комити: `4ba278b` (feat: DAQ Display Multi-Plot v1), `c74e7e3` (feat: FftUtil magnitudeSpectrum + SampledData decodeToNormalizedF32), `1ffac96` (chore: remove temporary thread-identity logging after Phase 4 verification), `2174008` (test: SampledData decode), `0655401` (test: FftUtil + AudioBufferToSampled)
  - Статус: ACTIVE (impl готов, unit тестове добавени и минаващи — Qt5/Qt6)
- **REQ-SW-PL-024** (AudioSource миграция от QDevIO към SampledData — нов нод
  взема името, старият → `_obsolete`):
  - Нов `AudioSourceDataModel` (registered name `AudioSource`) — SampledData поток
    `{"sample","Sample"}`; каптурата е в **dedicated worker thread**
    (`MicCaptureWorker`, moveToThread в модел-притежаван `QThread`); GUI нишката
    само keep-latest + `dataUpdated(0)`; connection-count gating — без свързан
    изход worker-ът дренира и не wrap-ва
  - Старият QDevIO mic → `AudioSourceDataModelObsolete` (registered
    `AudioSourceObsolete`, caption "(obsolete)") + `AudioWorkerObsolete` +
    `AudioNodeQdevIoConnectorObsolete` + `EventThreadPullObsolete` — rename-only,
    работи (база за benchmarking старо vs ново)
  - `AudioSourceDataModelUI` споделен и непроменен; enum `StartStop` е дефиниран
    в UI header-а (общ contract за двата нода)
  - Комити: `6ee9d3b` (refactor: rename old QDevIO mic + helpers),
    `42eb5fa` (feat: SampledData node + MicCaptureWorker), `0655401`
    (test: AudioBufferToSampled glue)
  - Статус: ACTIVE (impl готов, unit тестове добавени и минаващи — Qt5/Qt6);
    Qt5 (5.15.2) + Qt6
    (6.9.2) builds PASS + headless smoke PASS (capture в worker нишка → queued
    SampledData на GUI нишката, чисто start/stop, чисто унищожаване)
- **REQ-SW-PL-025** (DaqDisplayNode Multi-Plot v2 — физически decode + unit оси + ring buffer):
  - `SampledData::decodeToPhysical()` — header-only физически decode
    (`raw × amplitudeScale + amplitudeOffset`, БЕЗ normalization/центриране/clamp)
    + нови raw decoders в `SampledDecoder` (`rawS8..rawF64`, `decodeRawSample`);
    `decodeToNormalizedF32` остава непроменен
  - Unit оси — per-card `QValueAxis` заглавия от дескриптора: Time Domain →
    X `"Time (s)"` / Y `descriptor.unit` (fallback "normalized"/"amplitude");
    Frequency → X `"Frequency (Hz)"` / Y unit (или "Magnitude" в normalized
    режим); physical Y-обхват от min/max на декодираните стойности с ~5%
    padding (normalized картите запазват [-1, 1]); `mode` (normalized/physical)
    + `unitAxes` per-card, default за нови карти от `unit != "normalized"`
  - Worker-притежаван ring buffer — N-секундна плъзгаща се история на канал
    (raw bytes, default 10 s, `ringSeconds`); append на всеки compute pass,
    descriptor-change reset при sampleRate/канали/bytesPerFrame промяна, FFT от
    опашката (tail ≤4096), Time Domain показва целия прозорец (≤2000 точки);
    всичката работа на worker нишката (QThreadPool maxThreadCount=1) — GUI
    нишката само `series->replace()` + `axis->setRange()`
  - save()/restore() — нови опционални полета `ringSeconds`, per-card
    `mode` + `unitAxes` с defaults (10.0 / "normalized" / true) — старите v1
    файлове се зареждат без промяна
  - Комити: `6c20617` (feat: DaqDisplayNode v2 — decodeToPhysical, unit axes,
    worker-owned ring buffer)
  - Статус: ACTIVE (impl готов, unit тестове отложени); Qt5 (5.15.2) + Qt6
    (6.9.2) builds PASS + ctest 6/6 green (и двете) + offscreen smoke PASS
    (int16 32767 → 32.767, float passthrough без clamp, unit axis заглавия,
    save/restore round-trip с v1 defaults)
- **REQ-SW-FW-008** (Lightweight Runtime Profiling Framework — нов framework-level
  модул `src/frame_work/base/src/perf/` в `namespace Daqster::Perf`):
  - `RollingStats` — фиксиран pre-allocated ring buffer (O(1) `add`, без heap
    алокация в hot path); `avg`/`min`/`max` on demand, `count` капнат на
    capacity, `reset()`
  - `Domain` — именуван runtime-toggleable профилиращ домейн: thread-safe
    `get(name)` get-or-create регистър, relaxed-atomic `enabled()`/`setEnabled()`,
    `record(stage, ns)` (no-op при изключен домейн), `flush()` (агрегира през
    `qCDebug(lcPerf)` + reset)
  - `Scope` — RAII таймер за синхронни блокове (нулев разход при изключен
    домейн); `Stopwatch` — `mark()`/`reset()` за async/event мерене (ns)
  - Макрота `PERF_SCOPE(dom, stage)` / `PERF_ENABLED(dom)` с двустепенно
    изключване: compile-time (`DAQSTER_ENABLE_PERF` CMake option, default ON →
    `((void)0)`/`false` при OFF, нула Perf символи в потребителите) + runtime
    (atomic flag live toggle)
  - Нова лог категория `lcPerf` = `"daqster.perf"` в `LogCategories.{h,cpp}`
  - Unit тестове `tests/framework/perf/perf_profiler_tests` **19/19** (RollingStats
    6, Domain 7, Stopwatch/Scope 6); Qt5 (5.15.2) + Qt6 (6.9.2) builds PASS +
    ctest 9/9 green (и двете) + `-DDAQSTER_ENABLE_PERF=OFF` build PASS
  - Комити: `6ad8e89` (PerfProfiler module), `2c31b4a` (lcPerf + CMake),
    `52a620c` (perf_profiler_tests)
- **GL blit display widget** (`VideoGLBlitWidget`, `DAQSTER_GL_BLIT=1`) — detached OpenGL display за `VideoOutputNode`: качва декодираните CPU кадри като YUV текстури (NV12 / YUV420P) и конвертира в RGB във fragment shader (QImage fallback за RGB формати). И на двете Qt версии; измерено CPU: Qt5 27.9% → 15.0-16.2%, Qt6 17.8-18.2%, GLBLIT ~50-330 µs, failures=0.
- **NV12-direct за Qt5 (REQ-SW-PL-020)** — `VideoFrameData` вече не е Qt6-gated; Qt5 source-ите емитират OWNED копие на декодирания кадър (`VideoCompat::frameToOwnedFrame` — NV12/YUV420P plane-ове memcpy в `QAbstractPlanarVideoBuffer`) на port 0 (video-frame), image port 1 конвертира on-demand, audio port 2 appended last. `VideoOutputNode` има dual input video-frame@0 / image@1 и на двете версии. **Преномерация:** на Qt5 image портът се мести от 0 на 1 — стари Qt5 графи с image@0 трябва да се пресвържат.
- **Perf резултати doc** — `tests/performance/performance-video-display-2026-08-13.md`: пълна методология, числа преди/след shadow, perf анализ, промени и следващи лостове.
- **Qt6 in-scene GPU video display (REQ-SW-PL-021)** — чекбоксът „GPU display" вече е **видим и на Qt6** (checked по подразбиране = detached `QVideoWidget`, запазено текущото поведение). При unchecked `VideoOutputNode` създава `QGraphicsVideoItem` като child на `NodeGraphicsObject`-а на node-а (`ensureSceneVideoItem()` — намира сцената през `QApplication::topLevelWidgets()` → `GraphicsView` → `DataFlowGraphicsScene`) и кадрите се подават през `VideoCompat::presentFrame(item->videoSink(), frame)` — GPU път без QImage копие; софтуерният QLabel път остава автоматичен fallback. Detached прозорците се затварят при unchecked; in-scene item-ът се трие при toggle/дисконект. Perf бейджът следва само detached прозорците. Qt5 пътищата са непроменени. Dev driver: `DAQSTER_SCENE_VIDEO=1` uncheck-ва чекбокса за headless проверка. Комити: `63c7f78`, `a04f05e`, `3a0686e`, `ba7561f`. Smoke: Qt6 in-scene (кадрите се рендерират в сцената) + Qt6 detached + Qt5 GL blit regression — PASS без crash; ctest 9/9 (Qt5 + Qt6).

### Changed
- **Directory Restructuring**:
  - `src/external_libs/` → `src/plugins/external_libs/` (всички external libs са под plugins)
  - `src/plugins/node_editor/` → разделяне на `node_editor_widget/` + `node_editor_app/`
  - `.gitmodules` paths актуализирани
- **nodeeditor target**: Променен от `nodes` на `QtNodes` (pin commit `4709573`)
- **cmake/ComponentTemplates.cmake**: `create_external_library()` path → `src/plugins/external_libs/`
- **CI Workflow**: Добавени Python patch стъпки за qtrest install fix (cmake_install.cmake patching)
- **Architecture docs** актуализирани за новата структура
- **Documentation**:
  - Plugins hub (`docs/plugins/README.md`) + fix на plugin documentation links в INDEX/Architecture (`b5c204f`)
  - Demo plugin README — документирани video nodes и optional OpenCV (`e2c4925`)
  - REQ-SW-PL-018/PL-019 documentation refs backfill; plugin version alignment 0.3.0 → 0.2.0 в `project()` за `demo_nodeditor_nodes` и `node_editor_ide` (inert metadata, съответства на runtime 0.2.0) (`ed8b334`)
- **StreamUrlValidator** — извлечен от `StreamSourceNode` като самостоятелен header-only helper (`3048fbd`) за unit-testability (валидация на http/https/rtsp stream URL)
- **Process**: mandatory branch-per-work-item clause в AGENTS.md (`bee28c4`); trunk-based-lite from master (`547401c`); master consolidation (PR #21 `3c8c47f` merged, phase3 branch изтрит, PL-020 rebased onto master)
- **Node drop shadows изключени (perf)** — nodeeditor default `DefaultStyle.json` вече е с `ShadowEnabled=false` и display нодовете (`VideoOutputNode`, `DaqDisplayNode`, `NumberDisplayDataModel`, `QDevIoDisplayModelObsolete` вкл. AudioDisplay) го форсират per-node. `QGraphicsDropShadowEffect` blur-ът се изпълнява при всеки scene repaint и струваше ~46% CPU при video playback (измерено Qt6 36% → 17.6%).
- **GL blit става DEFAULT за Qt5 (REQ-SW-PL-021)** — `VideoOutputNode` избира display backend-а така: Qt5 = GL blit **ON** по подразбиране (най-бързият път), Qt6 = native `QVideoWidget` (GL blit OFF). `DAQSTER_GL_BLIT` е debug override само за стартиране: `=0` форсира софтуерен път (и Qt5), `=1` форсира GL (и Qt6). Поправен бъг „=0 включва": стойността вече има значение (`qEnvironmentVariableIntValue`).
- **Auto-fallback при недостъпен GL (REQ-SW-PL-021)** — при неуспех на GL контекста (`glPlatformAvailable()` pre-probe преди създаване на widget + async `QOpenGLWidget::isValid()` check след show) `VideoOutputNode` логва `GL fallback: <причина>` и превключва на софтуерния път (Qt5: кадър → QImage → QLabel; Qt6: native QVideoWidget). Видеото продължава да се показва (25 fps) без crash. Session-guard `m_glFailed` предотвратява повторно създаване на счупен прозорец.
- **UI toggle „GPU display" (REQ-SW-PL-021)** — чекбокс в `VideoOutputNode`, видим + checked по подразбиране и на двете Qt версии. Qt5: checked → detached GL blit, unchecked → софтуерен QLabel. Qt6: checked → detached прозорец (`QVideoWidget` или GL blit при `DAQSTER_GL_BLIT=1`), unchecked → in-scene `QGraphicsVideoItem` (вж. „Qt6 in-scene GPU video display" в Added). Потребителят може да го включва/изключва на живо — прилага се при следващия кадър, без crash и без загуба на видео. Приоритет: UI има последната дума след старт; env var-ът е само за стартиране.
- **Qt5 софтуерен display path за video-frame порт** — преди това Qt5 без GL blit не показваше кадрите на port 0 (video-frame); сега frameToImage → QLabel показва видеото (измеримо: `DAQSTER_GL_BLIT=0` ≈ 34-36% CPU).
- **Video source порт преномерация на Qt5** — виж NV12-direct по-горе; стари Qt5 графи, свързващи source port 0 (беше "image") с ImageData консуматор, губят връзката.

### Fixed
- **Конзолният `quit` не работеше от main app launcher-а (REQ-SW-APP-002, PUB-002)** —
  `QConsoleListener` се създаваше само вътре в `if (args.count() > 0)` клона на
  `main()` (след plugin load), затова при стартиране на Daqster без аргументи
  (основния launcher → `QMainWindow` + toolbar) stdin `quit\n` беше игнориран и
  приложението не излизаше. Сега `QConsoleListener` се създава **безусловно**
  преди `if (args.count() > 0)` — `quit` работи на всички стартови пътища (main
  launcher, single-arg plugin, multi-arg spawner). Проверено с PTY harness (сам
  процес, без child spawn-ове, DISPLAY=:0): `quit\n` извежда процеса за
  0.12-0.17 s (Qt5 + Qt6, с и без `NodeEditorIde`), exit 0. Коммит `9893d35`.
- **QConsoleListener busy-spin при stdin EOF (REQ-SW-APP-002, PUB-002)** — при
  стартиране на Daqster без blocking stdin (`< /dev/null`, затворен stdin от
  IDE/launcher, или приключил pipe) `QSocketNotifier` на `fileno(stdin)`
  зацикляше: EOF е перманентно "readable", всяко активиране четеше празен ред
  и емитираше празно `newLine("")` → **busy-spin ~181-183% idle CPU**. Сега при
  първото празно четене (`readLine()==0` ⇒ `read()` върна 0 ⇒ EOF) notifier-ът
  се disable-ва; жив терминал/pipe с данни не се засяга (при липса на вход fd
  не е readable и notifier-ът не се активира). Windows: `QWinEventNotifier` се
  disable-ва при `getline()==""` + `std::cin.eof()`. Измерено след fix-а: idle
  CPU без blocking stdin **0.0%** (Qt6 и Qt5, instantaneous /proc stat deltas;
  lifetime avg 3.8%/3.3% вкл. startup), срещу ~181-183% преди. Workaround-ът
  `tail -f /dev/null |` вече не е нужен. Коммит `6900e2c`.
- **Plugin-not-found / create-failure startup — процесът излиза сам (REQ-SW-APP-003)** —
  при `Daqster <plugin-name>` (single-arg) с несъществуващ plugin (нито hash, нито
  name match) или при намерен hash, но неуспешен `CreatePluginObject` (nullptr),
  `main.cpp` показваше `QMessageBox::critical` и след това продължаваше към
  `a.exec()` — процесът оставаше жив като ПРАЗЕН прозорец без функционалност,
  докато не се затвори ръчно. Сега и в двата failure path-а след затварянето на
  диалога `main()` връща `1` (exit code != 0) — процесът (вкл. child процесите,
  спавнати от launcher-а през `ApplicationsManager::StartApplication`) се затваря
  сам, без празен прозорец. Success path-ът (валиден plugin) и multi-arg
  spawner-ът са непроменени. Проверено ръчно (Qt5 + Qt6, DISPLAY=:0): диалог се
  показва, след Return/OK процесът излиза с код 1; `NodeEditorIDE` стартира и
  конзолният `quit` продължава да работи (REQ-SW-APP-002).
- **Plugin launch fixes**:
  - Toolbar пуска plugins по име вместо stale hash
  - Prune на persisted plugin entries с несъответстващ файлов hash при load
  - Явна грешка (qCCritical + QMessageBox) при неуспешно зареждане на plugin
  - AppSelectionDialog съхранява visibility по plugin name
- **Video нодове** — компилация на Qt5 и Qt6 (VideoCompat helpers: connectPlayerError/connectCameraError, variantToInt за Qt5 QVariant, QOverload за error signal)
- **Dependency graph** — edges следват nodes при drag, viewport се fit-ва при resize
- **Requirements Manager** — dedup на requirement файлове, достижими през множество roots, предупреждение при добавяне на overlapping root
- **NumericType::numberAsText()** — fix за ambiguous overload при int тип: явно cast до `double` с precision 0, предотвратява показване на hex/placeholder стойности вместо числа
- **VideoTransformNode Flip (Qt5)** — вертикалният flip ползваше `QImage::mirrored()` с грешни параметри на Qt5; поправен (`0cf6d19`), открит от новите unit тестове
- **Exporter CSV header** — колона "Repo" в CSV header-а на traceability matrix експорта беше с несъответстващо име; поправен (`9d90fff`)
- **LoggingSystem.md** — symlink заменен с regular file за GitHub docs rendering (`b247046`)
- **VideoOutputNode (Qt6 GPU path)** — премахната per-frame QImage конверсия + QLabel update + ImageData output когато GPU пътят (detached QVideoWidget) е активен; QLabel-ът показва статичен placeholder ("GPU display active — see detached window") веднъж; QImage конверсията + ImageData output се изпълняват само когато има downstream consumer connected на output port 0 (проследявано чрез `outputConnectionCreated`/`outputConnectionDeleted` + `m_outputConnectionCount`). Това премахва двойната CPU работа при активен GPU път.
- **VideoOutputNode (Qt6, detached popup)** — detached QVideoWidget popup-ът вече се затваря при премахване на port-0 connection (`inputConnectionCreated`/`inputConnectionDeleted` + `m_videoInputConnected` guard в `setInData()`). Преди това disconnect не спираше source player-а и кадрите продължаваха да пристигат в `setInData()`, възкресявайки popup-а. Също: null-check на `m_videoWidget` преди `presentFrame()` и спиране на излъчването на `ImageData` с null QImage (`d86b095`)
- **Temporary Qt6 video diagnostics** — qDebug логване на `QVideoFrame` (isValid, surfaceFormat().pixelFormat, handleType, map(), toImage()) в `VideoFileSourceNode`/`StreamSourceNode` onFrameAvailable (първите 10 кадъра) + еднократен dump на първия валиден кадър в `/tmp/qt6_frame_dump.png`. ВРЕМЕНЕН код — да се премахне след диагностиката (`abd46db`)
- **Тихо аудио на Qt6 (REQ-SW-PL-022)** — `VideoFileSourceNode` и `StreamSourceNode` създаваха `QMediaPlayer` без `QAudioOutput`; Qt6 изисква `setAudioOutput()` (иначе аудиото се декодира, но не се рутира). Добавен `QAudioOutput` member + `setAudioOutput()`, гарднат с `#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)`; Qt5 поведението непроменено (`12076a2`)
- **`[PERF]` конзолен ред не излизаше (REQ-SW-PL-027)** — `VideoOutputNode::logPerfLine()` логваше през `qCDebug(lcPerf)`, но категорията `daqster.perf` е изключена по подразбиране в `LogManager` → редът се филтрираше. Преминато на `qInfo().noquote()` (Info ниво, без категория — винаги излиза, като FFmpeg `[INF]` редовете); форматът на реда (formatPerfLine) е запазен
- **Perf overlay бейджът не се виждаше (REQ-SW-PL-027, Qt6)** — бейджът беше `QLabel` child на detached `QVideoWidget`; `QVideoWidget` рендерира видеото в отделен native слой (RHI swapchain) и не композира child widget-и върху него → бейджът оставаше невидим. Мигриран към отделен top-level frameless tool прозорец (`Qt::Tool | FramelessWindowHint | WindowStaysOnTopHint`, `WA_TransparentForMouseEvents`, полупрозрачен фон), който следва позицията на видео прозореца (горе-ляво, offset ~4,4) при създаване и на всеки ~500 ms refresh; `show()`/`hide()` според `domain.enabled()`; затваря се при `inputConnectionDeleted`/деструктор
- **Detached GL blit прозорецът не се появяваше при повторно закачане (Qt5 + Qt6 image порт)** — разкачането криеше GL прозореца (null клон на `updateDisplay()`), но `ensureGlWidget()` връщаше рано при съществуващ-но-скрит widget, а Qt5 `inputConnectionDeleted()` беше no-op → прозорецът не се връщаше при повторно закачане. `ensureGlWidget()` вече re-show-ва скрит widget и Qt5 disconnect премахва прозореца като Qt6. Проверено ръчно и на двете версии.
- **Qt5 видео throttled до 1 fps при видим GL прозорец** — на NVIDIA GLX default swap interval throttled `QOpenGLWidget` repaint-ите до ~1 Hz (възпроизведено с минимален standalone widget), което задушаваше видео пайплайна до 1 fps. `VideoGLBlitWidget` задава `QSurfaceFormat::setSwapInterval(0)` (+ default format) → свободни presents → 25 fps.

### Known issues
- **VC-1 Advanced видео на Qt6 (known issue)** — VC-1 Advanced профил съдържание (напр. `50MB_1080P_THETESTDATA.COM_AVI.avi`, ASF контейнер, mislabelled .avi) показва зелен екран на Qt6: Qt FFmpeg backend (QFFmpegMediaPlugin, libavcodec 61/FFmpeg 7.1) доставя NV12 кадри с изцяло нулева chroma плоскост (Cb=Cr=0) → YCbCr→RGB = плътен зелен (0,226,0). Доказано със standalone probe без Daqster presentation код (system FFmpeg декодира правилно; H.264 и Cinepak работят на Qt6; Qt5 работи via GStreamer). Не е fixable в Daqster код — upstream Qt 6.9.2 bug. Открито на 2026-08-09.

### Removed
- `src/plugins/node_editor/` — монолитен plugin (заменен от widget + app)
- `src/external_libs/` — празна директория премахната

- **Framework Architecture Refactoring** - голям рефакторинг за извличане на reusable компоненти:
  - **Platform Abstraction Layer** (`frame_work/base/src/platform/`):
    - `ShutdownHandler` - абстрактен базов клас за graceful shutdown
    - `UnixShutdownHandler` - SIGINT/SIGTERM signal handling за Unix/Linux (self-pipe)
    - `WindowsShutdownHandler` - Windows console events (SetConsoleCtrlHandler)
    - `QConsoleListener` - stdin-базиран quit/exit handler (крос-платформен, в Daqster приложението)
  - **Process Management Layer** (`frame_work/base/src/process/`):
    - `QProcessManager` - generic базов клас за управление на child процеси
    - Handle-based process tracking
    - Graceful terminate с force-kill fallback
    - Virtual hooks за customization (setupProcessEnvironment, onAllProcessesFinished)
  - **ApplicationsManager Refactoring**:
    - Наследява от `Daqster::QProcessManager`
    - Backward compatibility запазена (type aliases, event mappings)
    - Daqster-specific environment setup (plugin paths, AppImage detection, XDG directories)
    - Signal forwarding (ProcessEvent → ApplicationEvent)
  - **Cross-platform Support**:
    - Платформено-независим shutdown механизъм
    - Правилно Ctrl+C handling на Windows и Unix
    - Graceful процес терминация на всички платформи
- **PluginDependencyManager System** - автоматична система за управление на plugin dependencies
  - `cmake/PluginDependencyManager.cmake` - основна система за dependency management
  - `cmake/PluginExamples.cmake` - примери за използване на системата
  - `docs/PluginDependencyManagement.md` - подробна документация
- **Automatic Plugin Management**:
  - Автоматично откриване на Qt модули, external библиотеки и packages
  - Условно компилиране на plugins според наличните dependencies
  - Подробна debug информация за plugin статус
  - Поддръжка за Qt5 (пълна функционалност) и Qt6 (ограничена функционалност)
- **External Library Integration**:
  - Qt5: NodeEditor + QtRest библиотеки включени
  - Qt6: External библиотеки изключени заради compatibility проблеми
- **Enhanced Build System**:
  - `register_plugin()` функция за лесно регистриране на plugins
  - Автоматично проверяване на dependencies
  - Условно включване на plugin subdirectories
  - Build configuration и plugin status summaries

## [0.2.0] - 2025-09-18

### Added
- **Unified AppImage build system** - единен скрипт `tools/create_appimage.sh` за локално и CI създаване на AppImage
- **GitHub Actions CI/CD** - автоматизирани билдове и releases с AppImage артефакти
- **Debug AppImage support** - отделни Debug и Release AppImage билдове
- **Comprehensive documentation**:
  - `docs/Architecture.md` и `docs/Architecture.en.md` с PlantUML диаграми
  - `docs/HowToDebugAppImage.md` - ръководство за дебъгване на AppImage
  - Обновени README файлове с подробни инструкции за билдове и environment променливи
- **Enhanced plugin system**:
  - Подобрено environment variable handling за child processes
  - Debug output за plugin discovery paths
  - AppImage detection и адаптивно поведение
- **Professional build types** - Debug (за разработка) и Release (за production)
- **Environment variables documentation** - пълно описание на всички променливи за plugin discovery, Qt environment и debugging

### Changed
- **AppImage creation process** - автоматизиран с unified скрипт, поддържа local и CI режими
- **Plugin launching** - подобрено за AppImage среда с правилно environment setup
- **CI/CD workflows** - опростени и оптимизирани за AppImage създаване
- **Documentation structure** - организирана в `docs/` директория с PlantUML диаграми

### Fixed
- **Plugin discovery в AppImage** - поправени environment variables за child processes
- **QML loading issues** - поправен QtCoinTrader plugin за правилно QML зареждане
- **CI permissions** - поправени execute permissions за AppImage скриптове
- **Plugin launching от меню** - работи правилно в AppImage среда

### Technical Details
- **Build system**: CMake 3.16+, Qt 5.15.2, AppImage packaging
- **Plugin architecture**: Dynamic loading с hash-based deduplication
- **Process isolation**: Plugins се стартират като отделни QProcess
- **Cross-platform**: Linux AppImage distribution готов

## [0.1.0] - 2025-08-29
### Added
- Единен CMake билд: top-level `CMakeLists.txt` и поддиректории за apps, frame_work, plugins, external_libs.
- `install()` цели за `Daqster`, `frame_work`, `NodeEditorPlugin`, `QtCoinTraderPlugin`.
- RPATH настройки за runtime намиране на библиотеки (`$ORIGIN/../lib`).
- **Professional plugin discovery system** с приоритетни пътища:
  - Build директория (най-висок приоритет за дебъг)
  - Environment variables: `DAQSTER_PLUGIN_DIR` (една директория) и `DAQSTER_PLUGIN_PATH` (множество директории)
  - User plugins: `~/.local/share/daqster/plugins`
  - System plugins: `/usr/lib/daqster/plugins` и `/usr/local/lib/daqster/plugins`
- **Hash-based plugin deduplication** - предотвратява дублиране на същите plugins в различни директории.
- **AppImage-ready config system** - config файл се създава в writable location (`~/.config/Daqster/daqster.ini`).
- README на български и отделно `README.en.md` на английски с подробни инструкции за plugin discovery.

### Changed
- Премахнати всички qmake `.pro` файлове.
- Обновен `README.md` към CMake-only инструкции.
- **Config файл location** - от build директория към writable location за AppImage compatibility.

### Notes
- Начална публична версия, подготвена за CI/CD настройка.
- **Professional plugin architecture** готова за distribution като AppImage или Flatpak.

[0.1.0]: https://github.com/samiavasil/Daqster/releases/tag/v0.1.0


