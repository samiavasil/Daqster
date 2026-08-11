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
  - Комити: `4ba278b` (feat: DAQ Display Multi-Plot v1), `c74e7e3` (feat: FftUtil magnitudeSpectrum + SampledData decodeToNormalizedF32), `1ffac96` (chore: remove temporary thread-identity logging after Phase 4 verification)
  - Статус: ACTIVE (impl готов, unit тестове отложени)
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
    `42eb5fa` (feat: SampledData node + MicCaptureWorker)
  - Статус: ACTIVE (impl готов, unit тестове отложени); Qt5 (5.15.2) + Qt6
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

### Fixed
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


