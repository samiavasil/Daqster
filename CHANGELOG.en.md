# Changelog
All notable changes to this project will be documented in this file.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and
the project uses [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- **Video nodes** (`src/plugins/demo_nodeditor_nodes/Sources/Video/`):
  - `VideoCompat.h` — Qt5/Qt6 multimedia abstraction (QVideoProbe vs QVideoSink, camera enumeration, media source assignment, playback-state signals)
  - `CameraSourceNode` — QCamera capture from default or user-selected device
  - `VideoFileSourceNode` — local video file playback (QMediaPlayer)
  - `StreamSourceNode` — HTTP/RTSP stream playback (QMediaPlayer)
  - `VideoOutputNode` — live preview of incoming frames (QLabel, pass-through)
  - `VideoModifierNode` → `VideoTransformNode` (REQ-SW-PL-019) — configurable transform node: 8 base operations (RGB Channel Swap, Grayscale, Invert, Brightness, Contrast, Blur, Flip, Sepia) + optional OpenCV operations (GaussianBlur, Canny, Threshold) when OpenCV is detected
  - Registered under the "Video" category in the demo node editor plugin
- **Requirements Manager plugin** (`src/plugins/requirements_manager/`):
  - Standalone application plugin with requirements tree (REQ-SW-002..008)
  - Navigation: auto-clear filter on link click + back/forward history
  - Search engine (REQ-SW-PL-011)
  - Multi-repository requirements view — merge of public/private requirements (REQ-SW-PL-012)
- **Dependency graph viewer** (REQ-SW-009) — interactive graph of dependencies between requirements
- **Traceability matrix** (REQ-SW-010) — traceability matrix view and export
- **Typed requirement IDs** — support for the `REQ-SW-<TYPE>-NN` scheme with format validation (with migration of existing IDs)
- **Sugiyama auto-layout** (REQ-SW-PL-016) — automatic layout for the dependency graph
- **Phase status indicators** (REQ-SW-PL-017) — phase/status indicators in the requirements main view
- **Plugin security & vendor trust store** (REQ-SW-FW-007) — new requirement for plugin security
- **Unit tests for REQ-SW-PL-011/012/017/018/019** (2026-08-07, Qt5 5.15.2 + Qt6 6.9.2 PASS):
  - requirements manager shared binary `requirements_manager_tests` **87/87** (TestSearchEngine 19, TestMerge 6, validator +4, TestParser +5 phaseStatus) + `requirements_manager_matrix_tests` 7/7 + `requirements_manager_exporter_tests` 7/7
  - video `demo_nodeditor_nodes_tests` **25/25** (16 VideoTransformOps + 9 StreamUrlValidator)
  - commits: `825a9b4` (PL-017), `dff234a` (PL-011), `0429ea8` (PL-012), `3048fbd` (refactor: StreamUrlValidator), `0cf6d19` (fix: Qt5 vertical flip), `bac4503` (video suite), `9d90fff` (fix: exporter CSV header)
- **ArithmeticLogic node** (`BuiltInNodes/Operators/ArithmeticLogic/`):
  - `ExprParser` — recursive descent C++ expression evaluator (all C/C++ operators: `+`,`-`,`*`,`/`,`%`,`&`,`|`,`^`,`~`,`<<`,`>>`,`&&`,`||`,`!`,`==`,`!=`,`<`,`>`,`<=`,`>=`,`?:`)
  - `ArithmeticLogicModel` — configurable node: type (int/double), 2–8 inputs, expression field, optional strobe
  - Variables `a`–`h` map to input ports
- **Framework Architecture Refactoring** - major refactoring to extract reusable components:
  - **Platform Abstraction Layer** (`frame_work/base/src/platform/`):
    - `ShutdownHandler` - abstract base class for graceful shutdown
    - `UnixShutdownHandler` - SIGINT/SIGTERM signal handling for Unix/Linux (self-pipe)
    - `WindowsShutdownHandler` - Windows console events (SetConsoleCtrlHandler)
    - `QConsoleListener` - stdin-based quit/exit commands (cross-platform, in Daqster application)
  - **Process Management Layer** (`frame_work/base/src/process/`):
    - `QProcessManager` - generic base class for managing child processes
    - Handle-based process tracking
    - Graceful terminate with force-kill fallback
    - Virtual hooks for customization (setupProcessEnvironment, onAllProcessesFinished)
  - **ApplicationsManager Refactoring**:
    - Inherits from `Daqster::QProcessManager`
    - Backward compatibility preserved (type aliases, event mappings)
    - Daqster-specific environment setup (plugin paths, AppImage detection, XDG directories)
    - Signal forwarding (ProcessEvent → ApplicationEvent)
  - **Cross-platform Support**:
    - Platform-independent shutdown mechanism
    - Proper Ctrl+C handling on Windows and Unix
    - Graceful process termination on all platforms
- **PluginDependencyManager System** - automatic plugin dependency management system
  - `cmake/PluginDependencyManager.cmake` - core dependency management system
  - `cmake/PluginExamples.cmake` - usage examples for the system
  - `docs/PluginDependencyManagement.md` - comprehensive documentation
- **Automatic Plugin Management**:
  - Automatic detection of Qt modules, external libraries and packages
  - Conditional plugin compilation based on available dependencies
  - Detailed debug information about plugin status
  - Support for Qt5 (full functionality) and Qt6 (limited functionality)
- **External Library Integration**:
  - Qt5: NodeEditor + QtRest libraries included
  - Qt6: External libraries disabled due to compatibility issues
- **Enhanced Build System**:
  - `register_plugin()` function for easy plugin registration
  - Automatic dependency checking
  - Conditional plugin subdirectory inclusion
  - Build configuration and plugin status summaries
- **REQ-SW-PL-020** (zero-copy video frame display):
  - `VideoFrameData` — zero-copy shared data type (QVideoFrame Qt6 / QImage Qt5)
  - Dual-output source nodes (Qt6): `CameraSourceNode` and `VideoFileSourceNode` emit both `VideoFrameData` (zero-copy) and `ImageData` (legacy)
  - `VideoOutputNode` — Qt6 GPU display via detached `QVideoWidget`; Qt5 falls back to `QLabel` pixmap
  - `VideoCompat` — Qt5/Qt6 multimedia abstraction helpers (presentFrameCompat, presentImageCompat, connectPlayerError, connectCameraError, variantToInt, QOverload)
  - Windows cross-platform compliance (QStandardPaths, no Linux-only paths)
  - Commits: `b5c9651` (req), `157f34d` (VideoFrameData+shim), `085f63d` (VideoOutputNode), `0f92a9c` (CMake), `c873b43` (docs), `9d0f178` (AC/status)
  - Status: ACTIVE (impl done, unit tests deferred)
- (REQ-SW-PL-020) VideoFrameData and VideoOutputNode unit tests (PL-020 AC6) — e054396.
- **REQ-SW-PL-022** (unified sampled data transport & DAQ Display):
  - `SampledData` — unified NodeData type (`{"sample","Sample"}`) in `src/plugins/common/NodeDataTypes/`, QtCore-only, carrying QByteArray + per-channel `{name, SampleType}` + double sampleRate + `decodeToNormalized()`; `AudioData` = SampledData with `domain="audio"` (no separate class)
  - `SampledStreamDescriptor` — consolidation of `GenericStreamConfig` + `QDevIOStreamConfig`: extended `SampleType` enum (int8/uint8/int16/uint16/int24/uint24/int32/uint32/float32/float64), endianness, unit + amplitudeScale + amplitudeOffset, `domain` field ("audio"/"vibration"/"daq"/"ecg"/…), device id/source name, first-sample timestamp
  - Unified decoder convention — signed/unsigned divide by `(2^(bits-1) − 1)` with clamp to [-1,1], floats clamped (fix of the old GenericNumericTypes 32768/no-clamp convention); `AudioFrameDecoder` gained a QtCore-only `configure(SampleType, bits, endian)` overload
  - SampledData audio output port on `VideoFileSourceNode` + `StreamSourceNode` (appended LAST — Qt6 port 2, Qt5 port 1 — old saved graphs keep their indexes); capture: Qt6 `QAudioBufferOutput` (6.8+) / Qt5 `QAudioProbe`; buffers wrapped in `shared_ptr<SampledData>` and emitted gated on connection count
  - **DAQ Display node** — `Displays/DaqDisplay/DaqDisplayNode.{h,cpp}`: real Qt Charts waveform + FFT for any sampled source (audio/DAQ/sensors); plot slots `std::function<QVector<float>(const SampledData&)>` (JIT-ready), v1 built-ins identity + FFT; `GenericDisplayNode` is a legacy alias, `AudioDisplayModel::configureAudioView` no-op replaced with real UI config
  - Commits: `3929326` (req), `12076a2` (Qt6 audio fix), `cf5d0ae` (SampledData+descriptor), `6395220` (decoder convention), `75e291c` (audio port), `c5559b0` (DAQ Display), `8c56e83` (Qt5/Qt6 build fixes)
  - Status: ACTIVE (impl done, unit tests deferred)
- **REQ-SW-PL-023** (DaqDisplayNode Multi-Plot v1):
  - `DaqDisplayNode` — плъгин за мултиплейт plotting чрез Qt Charts, FFT за всякarn sampled источник (audio/DAQ/sензори), per-plot channel/type, off-GUI compute via `QThreadPool`, save/restore via `QDataStream`
  - `FftUtil` — утилитарна библиотека за FFT-раундлайн, `magnitudeSpectrum`, `decodeToNormalizedF32`
  - Комити: `4ba278b` (feat: DAQ Display Multi-Plot v1), `c74e7e3` (feat: FftUtil magnitudeSpectrum + SampledData decodeToNormalizedF32), `1ffac96` (chore: remove temporary thread-identity logging after Phase 4 verification)
  - Status: ACTIVE (impl готов, unit тестове отложени)
- **REQ-SW-PL-024** (AudioSource migration from QDevIO to SampledData — new node
  takes the name, old one → `_obsolete`):
  - New `AudioSourceDataModel` (registered name `AudioSource`) — SampledData
    stream `{"sample","Sample"}`; capture runs on a **dedicated worker thread**
    (`MicCaptureWorker`, moveToThread into a model-owned `QThread`); the GUI
    thread only keeps the latest shared_ptr and emits `dataUpdated(0)`;
    connection-count gating — when no output is connected the worker drains and
    does not wrap
  - Old QDevIO mic → `AudioSourceDataModelObsolete` (registered
    `AudioSourceObsolete`, caption "(obsolete)") + `AudioWorkerObsolete` +
    `AudioNodeQdevIoConnectorObsolete` + `EventThreadPullObsolete` — rename-only,
    still works (baseline for old-vs-new benchmarking)
  - `AudioSourceDataModelUI` shared and unchanged; the `StartStop` enum is
    defined in the UI header (single contract for both nodes)
  - Commits: `6ee9d3b` (refactor: rename old QDevIO mic + helpers),
    `42eb5fa` (feat: SampledData node + MicCaptureWorker)
  - Status: ACTIVE (impl done, unit tests deferred); Qt5 (5.15.2) + Qt6 (6.9.2)
    builds PASS + headless smoke PASS (capture on the worker thread → queued
    SampledData delivered on the GUI thread, clean start/stop, clean destruction)
- **REQ-SW-PL-025** (DaqDisplayNode Multi-Plot v2 — physical decode + unit axes + ring buffer):
  - `SampledData::decodeToPhysical()` — header-only physical decode
    (`raw × amplitudeScale + amplitudeOffset`, NO normalization/centering/clamp)
    plus new raw decoders in `SampledDecoder` (`rawS8..rawF64`, `decodeRawSample`);
    `decodeToNormalizedF32` stays unchanged
  - Unit axes — per-card `QValueAxis` titles from the descriptor: Time Domain →
    X `"Time (s)"` / Y `descriptor.unit` (fallback "normalized"/"amplitude");
    Frequency → X `"Frequency (Hz)"` / Y unit (or "Magnitude" in normalized
    mode); physical Y range from min/max of decoded values with ~5% padding
    (normalized cards keep [-1, 1]); per-card `mode` (normalized/physical) +
    `unitAxes`, new-card default from `unit != "normalized"`
  - Worker-owned ring buffer — N-second rolling per-channel history (raw bytes,
    default 10 s, `ringSeconds`); append on every compute pass, descriptor-change
    reset on sampleRate/channels/bytesPerFrame change, FFT from the ring tail
    (≤4096), Time Domain shows the whole window (≤2000 points); all work on the
    worker thread (QThreadPool maxThreadCount=1) — the GUI thread only does
    `series->replace()` + `axis->setRange()`
  - save()/restore() — new optional fields `ringSeconds`, per-card `mode` +
    `unitAxes` with defaults (10.0 / "normalized" / true) — old v1 files load
    unchanged
  - Commits: `6c20617` (feat: DaqDisplayNode v2 — decodeToPhysical, unit axes,
    worker-owned ring buffer)
  - Status: ACTIVE (impl done, unit tests deferred); Qt5 (5.15.2) + Qt6 (6.9.2)
    builds PASS + ctest 6/6 green (both) + offscreen smoke PASS (int16 32767 →
    32.767, float passthrough without clamp, unit axis titles, save/restore
    round-trip with v1 defaults)

### Changed
- **ChatGraphModel.h** moved from `node_editor_ide/` to `BuiltInNodes/Library/types/` (shared library) for generality
- **Documentation**:
  - Plugins hub (`docs/plugins/README.md`) + fixed plugin documentation links in INDEX/Architecture (`b5c204f`)
  - Demo plugin README — documented video nodes and optional OpenCV (`e2c4925`)
  - REQ-SW-PL-018/PL-019 documentation refs backfill; plugin version alignment 0.3.0 → 0.2.0 in `project()` for `demo_nodeditor_nodes` and `node_editor_ide` (inert metadata, matching runtime 0.2.0) (`ed8b334`)
- **StreamUrlValidator** — extracted from `StreamSourceNode` as a standalone header-only helper (`3048fbd`) for unit-testability (http/https/rtsp stream URL validation)
- **Process**: mandatory branch-per-work-item clause in AGENTS.md (`bee28c4`); trunk-based-lite from master (`547401c`); master consolidation (PR #21 `3c8c47f` merged, phase3 branch deleted, PL-020 rebased onto master)

### Fixed
- **Plugin launch fixes**:
  - Toolbar launches plugins by name instead of stale hash
  - Prune persisted plugin entries with mismatched file hash on load
  - Fail loudly (qCCritical + QMessageBox) when a requested plugin fails to load
  - AppSelectionDialog persists visibility by plugin name
- **Video nodes** — Qt5 and Qt6 compilation fixes (VideoCompat helpers: connectPlayerError/connectCameraError, variantToInt for Qt5 QVariant, QOverload for the error signal)
- **Dependency graph** — edges follow nodes on drag, viewport fits on resize
- **Requirements Manager** — dedup requirement files reachable via multiple roots, warn when adding an overlapping root
- **NumericType::numberAsText()** — fixed ambiguous overload for int type: explicit cast to `double` with precision 0, prevents displaying hex/placeholder values instead of numbers
- **VideoTransformNode Flip (Qt5)** — the vertical flip used `QImage::mirrored()` with wrong parameters on Qt5; fixed (`0cf6d19`), found by the new unit tests
- **Exporter CSV header** — the "Repo" column in the traceability matrix CSV export header had a mismatched name; fixed (`9d90fff`)
- **LoggingSystem.md** — symlink replaced with regular file for GitHub docs rendering (`b247046`)
- **VideoOutputNode (Qt6 GPU path)** — removed per-frame QImage conversion + QLabel update + ImageData output when the GPU path (detached QVideoWidget) is active; the in-node QLabel shows a static placeholder ("GPU display active — see detached window") once; QImage conversion + ImageData output now run only when a downstream consumer is connected to output port 0 (tracked via `outputConnectionCreated`/`outputConnectionDeleted` + `m_outputConnectionCount`). Eliminates the doubled CPU work observed when the GPU display is active.
- **VideoOutputNode (Qt6, detached popup)** — the detached QVideoWidget popup now closes when the port-0 input connection is removed (`inputConnectionCreated`/`inputConnectionDeleted` + `m_videoInputConnected` guard in `setInData()`). Previously disconnecting the edge did NOT stop the source player, so frames kept arriving in `setInData()` and lazily re-created the detached widget (`ensureVideoWidget()`), resurrecting the popup; `inputConnectionDeleted()` was never implemented. On disconnect the widget is hidden + deleteLater + nulled, state is reset, the QLabel placeholder is restored. Also: defensive null-check of `m_videoWidget` before `presentFrame()` and stopped emitting `ImageData` wrapping a null `QImage` (`d86b095`). Qt5 unaffected (`#if QT_VERSION >= 6`).
- **Temporary Qt6 video diagnostics** — qDebug logging of `QVideoFrame` (isValid, surfaceFormat().pixelFormat, handleType, map(), toImage()) in `VideoFileSourceNode`/`StreamSourceNode` onFrameAvailable (first 10 frames) + one-time dump of the first valid frame to `/tmp/qt6_frame_dump.png`. TEMPORARY code — remove after diagnosis (`abd46db`)
- **Silent audio on Qt6 (REQ-SW-PL-022)** — `VideoFileSourceNode` and `StreamSourceNode` created `QMediaPlayer` without `QAudioOutput`; Qt6 requires `setAudioOutput()` (otherwise audio decodes but is never routed). Added `QAudioOutput` member + `setAudioOutput()`, guarded with `#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)`; Qt5 behavior unchanged (`12076a2`)

### Known issues
- **VC-1 Advanced video on Qt6 (known issue)** — VC-1 Advanced content (e.g. `50MB_1080P_THETESTDATA.COM_AVI.avi`, ASF container mislabelled .avi) shows a green screen on Qt6: the Qt FFmpeg backend (QFFmpegMediaPlugin, libavcodec 61/FFmpeg 7.1) delivers NV12 frames with an ENTIRELY zero chroma plane (Cb=Cr=0) → YCbCr→RGB = solid green (0,226,0). Proven with a standalone probe containing no Daqster presentation code (system FFmpeg decodes correctly; H.264 and Cinepak work on Qt6; Qt5 works via GStreamer). Not fixable in Daqster code — upstream Qt 6.9.2 bug. Discovered 2026-08-09.

## [0.2.0] - 2025-09-18

### Added
- **Unified AppImage build system** - single script `tools/create_appimage.sh` for local and CI AppImage creation
- **GitHub Actions CI/CD** - automated builds and releases with AppImage artifacts
- **Debug AppImage support** - separate Debug and Release AppImage builds
- **Comprehensive documentation**:
  - `docs/Architecture.md` and `docs/Architecture.en.md` with PlantUML diagrams
  - `docs/HowToDebugAppImage.md` - guide for debugging AppImage
  - Updated README files with detailed build and environment variable instructions
- **Enhanced plugin system**:
  - Improved environment variable handling for child processes
  - Debug output for plugin discovery paths
  - AppImage detection and adaptive behavior
- **Professional build types** - Debug (for development) and Release (for production)
- **Environment variables documentation** - complete description of all variables for plugin discovery, Qt environment and debugging

### Changed
- **AppImage creation process** - automated with unified script, supports local and CI modes
- **Plugin launching** - improved for AppImage environment with proper environment setup
- **CI/CD workflows** - simplified and optimized for AppImage creation
- **Documentation structure** - organized in `docs/` directory with PlantUML diagrams

### Fixed
- **Plugin discovery in AppImage** - fixed environment variables for child processes
- **QML loading issues** - fixed QtCoinTrader plugin for proper QML loading
- **CI permissions** - fixed execute permissions for AppImage scripts
- **Plugin launching from menu** - works correctly in AppImage environment

### Technical Details
- **Build system**: CMake 3.16+, Qt 5.15.2, AppImage packaging
- **Plugin architecture**: Dynamic loading with hash-based deduplication
- **Process isolation**: Plugins launched as separate QProcess
- **Cross-platform**: Linux AppImage distribution ready

## [0.1.0] - 2025-08-29
### Added
- Unified CMake build: top-level `CMakeLists.txt` and subdirectories for apps, frame_work, plugins, external_libs.
- `install()` targets for `Daqster`, `frame_work`, `NodeEditorPlugin`, `QtCoinTraderPlugin`.
- RPATH settings for locating shared libraries at runtime (`$ORIGIN/../lib`).
- **Professional plugin discovery system** with priority-based paths:
  - Build directory (highest priority for debug)
  - Environment variables: `DAQSTER_PLUGIN_DIR` (single directory) and `DAQSTER_PLUGIN_PATH` (multiple directories)
  - User plugins: `~/.local/share/daqster/plugins`
  - System plugins: `/usr/lib/daqster/plugins` and `/usr/local/lib/daqster/plugins`
- **Hash-based plugin deduplication** - prevents loading duplicate plugins from different directories.
- **AppImage-ready config system** - config file is created in writable location (`~/.config/Daqster/daqster.ini`).
- README in Bulgarian and separate `README.en.md` in English with detailed plugin discovery instructions.

### Changed
- Removed all qmake `.pro` files.
- Updated `README.md` to CMake-only instructions.
- **Config file location** - from build directory to writable location for AppImage compatibility.

### Notes
- Initial public version prepared for CI/CD setup.
- **Professional plugin architecture** ready for distribution as AppImage or Flatpak.

[0.1.0]: https://github.com/samiavasil/Daqster/releases/tag/v0.1.0


