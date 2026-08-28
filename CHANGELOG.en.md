# Changelog
All notable changes to this project will be documented in this file.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and
the project uses [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- **REQ-SW-PL-028** (VideoEffectNode — per-effect GPU/CPU backend):
  - `VideoGLShaders.h` — shared GLSL source builders (`buildVertexSource`, `buildYuvFragmentSource`, `buildRgbaFragmentSource` extracted from `VideoGLBlitWidget.cpp` + new `buildEffectFragmentSource` with `u_flipY` and injectable effect body)
  - `VideoEffectOps.{h,cpp}` — `EffectSpec` registry (7 effects: brightness, contrast, grayscale, invert, sepia, channelSwap, flip) with CPU functions (delegating to `VideoTransformOps`) + GLSL body
  - `VideoEffectGLProcessor.{h,cpp}` — GPU backend: `QOpenGLContext` + `QOffscreenSurface` + `QOpenGLFramebufferObject`, Y/U/V upload with `GL_UNPACK_ROW_LENGTH`, `hasHardwareGL()` (llvmpipe/softpipe/SwiftShader detection, lazy cached)
  - `VideoEffectNode.{h,cpp}` — node model (port 0 in/out `VideoFrameData`) + **single node with an effect combo** (combo + `QStackedWidget`, like `VideoTransformNode`), runtime backend selection, parameter UI (slider/combo/info), save/load with clamps (backward compatible: `"effect"` = id + params)
  - **Blur + OpenCV effects (2026-08-26, commit `095981b`)** — added `blur` (box blur, radius 0..10) + `gaussianBlur`/`canny`/`threshold` (OpenCV, only with `HAVE_OPENCV`) as `CpuOnly` effects — 11 effects total; they cover all operations of the removed `VideoTransformNode`
  - Registered under the "Video" category in the demo node editor plugin
- **REQ-SW-PL-030** (FrameSampler — frame resampling):
  - `FrameSamplerNode.{h,cpp}` — standalone node (port 0 in/out `VideoFrameData`), "Every N-th frame" (1..1000) / "Max FPS" (1..120) modes, zero-copy passthrough (same `shared_ptr`), gate without emit on drop, save/load + counter/timer reset
  - Registered under the "Video" category in the demo node editor plugin
- **REQ-SW-PL-032** (VideoFrameData lazy QImage cache):
  - `VideoFrameData::asImage()` — lazy CPU QImage cache: converts at most once per frame, caches and shares the result between all CPU consumers (fan-out); `setFrame()` invalidates the cache; GUI-thread only (no mutex)
  - Consumers use the cache: `VideoEffectNode` CPU path + `VideoOutputNode` software display / downstream consumer path (`VideoCompat::frameToImage` → `asImage()`); `VideoGLBlitWidget` stays on `frameToImage` (raw `QVideoFrame`, not `VideoFrameData`)
- **REQ-SW-PL-032** (GPU-resident transport, Stage 2A — shared GL context + texture transport):
  - `VideoGLContextManager` (`src/plugins/common/GL/`) — process-wide shared `QOpenGLContext` + `QOffscreenSurface` (share group with the `QOpenGLWidget` display), lazy `hasHardwareGL()` (llvmpipe/softpipe/SwiftShader detection), `deleteTexture()`; GUI-thread only
  - `VideoFrameData::asTexture()` — lazy GPU texture cache: Y/U/V upload once per frame (NV12/YUV420P), the handle is cached and shared between all GPU consumers; `fromTexture()` wraps a GPU-resident RGBA texture (effect output); `isGpuResident()`/`isGpuRgba()`; textures are deleted in the destructor
  - `VideoEffectGLProcessor` uses the shared context (instead of a private one)
- **REQ-SW-PL-032** (GPU-resident effect chain, Stage 2B — texture-in/texture-out, no readback):
  - `VideoEffectGLProcessor::processTexture()` — binds the input textures from a `VideoTextureHandle` (no upload), selects the program by layout (nv12/420p/rgba), renders into an FBO and returns the RGBA texture (no `toImage()`); new `buildRgbaEffectFragmentSource()` (RGBA sampler + effect body + u_flipY/u_brightness/u_contrast); the output texture is created per call and ownership is handed to `fromTexture()`
  - `VideoEffectNode` GPU path: `asTexture()` → `processTexture()` → `fromTexture()` — a second effect in a chain consumes the texture directly (no upload/readback); the CPU fallback stays
  - `VideoGLBlitWidget::presentTexture()` — zero-copy display of a GPU-resident RGBA texture (bind, no upload/readback); `VideoOutputNode` GL blit path uses it for GpuRgba frames; the Qt6 native display does a readback at the boundary (`presentableFrame` → `asImage()`)
  - Smoke driver: `DAQSTER_AUTOSTART_EFFECT2=<effectId>` inserts a second VideoEffect node (GPU-resident chain)
- **Smoke drivers** (`NodeEditorIdeObject.cpp`): `DAQSTER_AUTOSTART_EFFECT=<effectId>` inserts a VideoEffect node between source and output; `DAQSTER_AUTOSTART_EFFECT2=<effectId>` inserts a second one; `DAQSTER_AUTOSTART_SAMPLER=1` inserts a FrameSampler
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
- **REQ-SW-PL-027** (Video Pipeline Instrumentation & Overlay — first consumer of REQ-SW-FW-008):
  - Domain getters: `Daqster::Perf::Domain::{avg,min,max,count}(stage)` (read-only, -1 when the stage has no samples) in `PerfProfiler.{h,cpp}`
  - Source instrumentation: `StreamSourceNode`, `VideoFileSourceNode`, `CameraSourceNode` record the inter-frame gap (`"source.frame_interval"`) + the wrap/emit segment (`"source.wrap_emit"`) and latch `handleType`/`pixelFormat` (HW/SW markers) in the `"video"` domain
  - Output instrumentation: `VideoOutputNode` records `"output.present"` (blit) + `"output.total"` and latches the incoming frame's markers
  - Qt6-only overlay + checkbox: "Perf" checkbox → `Domain::get("video").setEnabled()` + 500 ms `QTimer` + semi-transparent `QLabel` badge (child of the detached `QVideoWidget`, top-left): `HW|SW · fmt=… · handle=… · fps=… · gap=…ms · present=…ms · total=…ms`
  - `VideoPerfBadge.{h,cpp}` — pure QtCore-only `formatPerfBadge()` formatter (no widgets/QtMultimedia) + deterministic unit test (8 slots)
  - Removed the temporary `VideoDiag` diagnostic block; zero overhead at `DAQSTER_ENABLE_PERF=OFF`
  - Commits: `885ad11`, `5c0c831`, `7682bb1`, `3ecd5c0`
- **REQ-SW-PL-027 (periodic console line + self-CPU)** — copy-paste-able perf numbers on both Qt5 + Qt6:
  - `ProcessCpu` (`src/frame_work/base/src/perf/ProcessCpu.{h,cpp}`) — self-CPU sampler for the current process (Linux `/proc/self/stat` utime+stime in clock ticks / Windows `GetProcessTimes()` KernelTime+UserTime); first sample establishes the baseline and returns 0.0
  - Periodic 5 s console log in `VideoOutputNode` (both Qt5 + Qt6): `[PERF] video | SW | fmt=NV12 | handle=NoHandle | fps=30 | gap=…ms | present=…ms | total=…ms | cpu=…%` via `qCDebug(lcPerf)` (stable `[PERF] video` prefix for grep/copy-paste); the "Perf" checkbox now lives on both Qt versions
  - Qt5 display-path instrumentation (`"output.present"` around `updateDisplay()`, `"output.total"` around the whole image branch) + Qt5 source-node instrumentation (`"source.frame_interval"` gap + `"source.wrap_emit"` wrapping the QVideoFrame→QImage conversion)
  - `formatPerfLine()` pure QtCore-only formatter in `VideoPerfBadge.{h,cpp}` + deterministic unit tests (5 new slots, 15 total in `TestVideoPerfBadge`)
  - Commits: `0eb005b`, `7a71e12`, `fb0455d`, `952e416`
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
- **REQ-SW-FW-008** (Lightweight Runtime Profiling Framework — new framework-level
  module `src/frame_work/base/src/perf/` in `namespace Daqster::Perf`):
  - `RollingStats` — fixed pre-allocated ring buffer (O(1) `add`, no heap
    allocation in the hot path); `avg`/`min`/`max` on demand, `count` capped at
    capacity, `reset()`
  - `Domain` — named runtime-toggleable profiling domain: thread-safe `get(name)`
    get-or-create registry, relaxed-atomic `enabled()`/`setEnabled()`,
    `record(stage, ns)` (no-op when disabled), `flush()` (aggregates via
    `qCDebug(lcPerf)` + reset)
  - `Scope` — RAII timer for synchronous blocks (zero cost when the domain is
    disabled); `Stopwatch` — `mark()`/`reset()` for async/event measurement (ns)
  - Macros `PERF_SCOPE(dom, stage)` / `PERF_ENABLED(dom)` with two-level opt-out:
    compile-time (`DAQSTER_ENABLE_PERF` CMake option, default ON → `((void)0)`/
    `false` when OFF, zero Perf symbols in consumers) + runtime (atomic live
    toggle)
  - New logging category `lcPerf` = `"daqster.perf"` in `LogCategories.{h,cpp}`
  - Unit tests `tests/framework/perf/perf_profiler_tests` **19/19** (RollingStats
    6, Domain 7, Stopwatch/Scope 6); Qt5 (5.15.2) + Qt6 (6.9.2) builds PASS +
    ctest 9/9 green (both) + `-DDAQSTER_ENABLE_PERF=OFF` build PASS
  - Commits: `6ad8e89` (PerfProfiler module), `2c31b4a` (lcPerf + CMake),
    `52a620c` (perf_profiler_tests)
- **GL blit display widget** (`VideoGLBlitWidget`, `DAQSTER_GL_BLIT=1`) — detached OpenGL display for `VideoOutputNode` that uploads decoded CPU frames as YUV textures (NV12 / YUV420P) and converts to RGB in a fragment shader (QImage fallback for RGB formats). Enabled on both Qt versions; measured CPU: Qt5 27.9% → 15.0-16.2%, Qt6 17.8-18.2%, GLBLIT ~50-330 µs, failures=0.
- **NV12-direct for Qt5 (REQ-SW-PL-020)** — `VideoFrameData` is no longer Qt6-gated; Qt5 video sources emit an OWNED copy of the decoded frame (`VideoCompat::frameToOwnedFrame` — NV12/YUV420P planes memcpy'd into a `QAbstractPlanarVideoBuffer`) on port 0 (video-frame), image port 1 converts on demand, audio port 2 appended last. `VideoOutputNode` has the video-frame@0 / image@1 dual input on both Qt versions. **Renumbering note:** on Qt5 the image port moved from 0 to 1 — old saved Qt5 graphs with image@0 must reconnect.
- **Video display perf results doc** — `tests/performance/performance-video-display-2026-08-13.md`: full methodology, before/after shadow numbers, perf bottleneck analysis, changes and next levers.
- **REQ-SW-PL-034** (VideoOutputNode embedded effects — optional, default none):
  - `VideoOutputNode` gains **optional embedded effects** — the same 11 effects as `VideoEffectNode` (brightness, contrast, grayscale, invert, sepia, channelSwap, flip, blur + gaussianBlur/canny/threshold with `HAVE_OPENCV`), **no effect by default**
  - Combo with a leading "No effect" item (index 0) + `QStackedWidget` parameter pages (slider/flip/OpenCV); backend suffix "(GPU)"/"(CPU)" like `VideoEffectNode`
  - When no effect is selected the block in `setInData()` is **skipped entirely** — `asTexture()`/`asImage()` are never called, the zero-copy passthrough is byte-identical (AC 2)
  - GPU path: `asTexture()` → `VideoEffectGLProcessor::processTexture()` → `fromTexture()` (GpuRgba, zero-copy) with hardware GL; CPU path/fallback: `asImage()` → `EffectSpec::cpuApply` → `QVideoFrame`
  - `save()`/`load()`: `"effect"` id + params (`brightness`/`contrast`/`flipMode`/`blurRadius`/`gaussianKernel`/`cannyLow`/`cannyHigh`/`thresholdValue`) with clamps; old graphs without the `"effect"` key → "no effect" (backward compatible)
  - Tests: `demo_nodeditor_videooutput_tests` +4 (default no-effect passthrough, load→save round-trip, backward compat, effect transforms the frame); Qt5/Qt6 builds PASS + ctest 9/9 green (both)

### Changed
- **GpuRgba frames → GL blit widget on Qt6 (REQ-SW-PL-032, Stage 2C, perf)** — `VideoOutputNode` routes GpuRgba frames (effect outputs) to the GL blit widget (`presentTexture`, zero-copy bind, ~19 µs/display) on Qt6 too when hardware GL is available (`VideoGLContextManager::hasHardwareGL()`) — instead of the native `QVideoWidget` path (readback `glReadPixels` 8 MB + per-sink RHI upload 8 MB/display; 2 displays on an effect output → CPU 10% → 50%+). CPU/NV12 frames keep the per-Qt backend (Qt6: native `QVideoWidget`, Qt5: GL blit). `ensureVideoWidget(wantGlBlit)` selects the backend per frame and destroys the other widget on switch (no leak, no crash).
- **Node body repaint on data arrival is now opt-out (REQ-SW-PL-032, perf)** — `dataArrivalChangesWidget()` virtual in nodeeditor: video/LLM/console nodes (`VideoOutputNode`, `VideoEffectNode`, `CustomShaderNode`, `FrameSamplerNode`, `LLamaModelDataModel`, `ConsoleDataModel`) return `false` and skip the per-frame `node->update()` body repaint (widget content self-repaints via Qt); validation-state nodes (NumberDisplay, DaqDisplay, ...) keep the default `true`. nodeeditor commit: `43c1dea`
- **Validation nodes opt out + self-invalidate (REQ-SW-PL-032, perf)** — `setValidationState()` in nodeeditor now emits `requestNodeUpdate()` (the validation border self-repaints only on an actual state change); `NumberDisplay`, `DaqDisplay`, `GenericDisplay`, `Modulo`, `ArithmeticLogic` return `dataArrivalChangesWidget() == false`. nodeeditor commit: `566f6ba`
- **NumberDisplay geometry only on real resize (REQ-SW-PL-032, perf)** — `dataArrivalChangesGeometry() == false` + event filter on `m_wrapper`: `requestNodeUpdate()` (recomputeSize + moveConnections) is emitted only on `QEvent::Resize`, not on every data arrival; `recomputeSize()` only READS the widget size → no recursion
- **VideoOutputNode output port — zero-copy passthrough (REQ-SW-PL-032, fix)** — with a downstream consumer attached the output is `m_output = videoFrame` (the SAME shared `VideoFrameData`, residency preserved) — no QImage readback, no `QVideoFrame` re-wrap, no double-present (`updateDisplay()` removed from the block)
- **`VideoGLBlitWidget::presentYuvTexture()` (REQ-SW-PL-032, perf)** — presents the cached YUV textures from `asTexture()` directly (NV12/YUV420P programs, standard top-down quad) — no duplicate upload; falls back to `presentFrame()` on an invalid handle
- **`VideoFrameData` copy ctor/assignment = delete (REQ-SW-PL-032, fix)** — frames are shared only via `shared_ptr`, never copied (GpuRgba copies were a dead end); no move semantics were present
- **`VideoEffectGLProcessor::process()` removed (REQ-SW-PL-032, refactor)** — dead code (FBO readback trap): no callers, only `processTexture()` is used; `uploadFrame()`/`classifyYuv()`/the `m_texY/U/V/UV` textures/`m_useNv12` removed too
- **Design refinement (REQ-SW-PL-028/029/032, 2026-08-25)** — documentation-only:
  - PL-028: refactor to a SINGLE `VideoEffectNode` with a combobox for effect selection + parameters/config (instead of 7 separate subclasses); EffectSpecs stay in `VideoEffectOps.h/.cpp`
  - PL-032: refined design — lazy caches (`asImage()`/`asTexture()`), node residency preferences (Option C), GPU-resident transport (Path B), Qt6-first/Qt5-after, format NV12 → Y+UV → YUV→RGB+effect → RGBA
  - PL-029: confirmed standalone node (not embedded in `VideoEffectNode`) — different UI (GLSL editor + compile + error log), extensible to DAQ/other types
- **VideoEffectNode combobox refactor (REQ-SW-PL-028 AC 4/5, implemented)** — the 7 per-effect subclasses become deprecated aliases; a single `VideoEffectNode` with combo + `QStackedWidget` (7 pages: brightness slider, contrast slider, grayscale/invert/sepia/channelSwap info, flip combo). `setEffect(id)` selects by id (unknown → index 0); `save()`/`load()` format unchanged (`"effect"` = id + params) — backward compatible. The smoke driver (`DAQSTER_AUTOSTART_EFFECT`) adds a single `VideoEffect` node and sets the effect via `load()`. Commits: `e84f6d0`, `42bb57a`, `9fb46b9`
- **VideoFrameData lazy QImage cache (REQ-SW-PL-032 AC 1/2/3 CPU part, implemented)** — `asImage()` converts once per frame and caches; `setFrame()` clears the cache; consumers (`VideoEffectNode` CPU path, `VideoOutputNode` software/consumer path) use the cache instead of `VideoCompat::frameToImage`. Commits: `3f9ec84`, `093b557`
- **GPU-resident transport Stage 2A (REQ-SW-PL-032 AC 1/5, implemented)** — `VideoGLContextManager` (shared GL context, share group with the display), `VideoFrameData::asTexture()` (lazy Y/U/V upload, cached handle) + `fromTexture()` (GPU-resident RGBA wrap) + `isGpuResident()`; `VideoEffectGLProcessor` migrated to the shared context. Commits: `6cb1aaa`, `803e4f6`, `9a39006`
- **GPU-resident effect chain Stage 2B (REQ-SW-PL-032 AC 5/7, implemented for the effect chain)** — `VideoEffectGLProcessor::processTexture()` (texture-in → texture-out, no `toImage()` readback; program cache key with layout ∈ {nv12, 420p, rgba}; new `buildRgbaEffectFragmentSource()`); `VideoEffectNode` GPU path `asTexture()` → `processTexture()` → `fromTexture()` — a second effect consumes the texture directly; `VideoGLBlitWidget::presentTexture()` — zero-copy display of the RGBA texture; the Qt6 native display does a readback at the boundary (Stage 2C). **PERF (Qt5 GL blit, sepia): cpu 62.2% → 16.9%, present 7.1ms → 0.1ms, GLBLIT avg 322us → 18.8us (fmt=Texture(RGBA)); 2 effects (sepia+invert) cpu 19.0%; Qt6: 34% → 25% (1 effect), 26.8% (2 effects).** Commits: `b6af20e`, `d1a5e7a`
- **Phase 3 — migration to a single VideoFrameData type (REQ-SW-PL-032, 2026-08-26)**:
  - **Image ports removed** from the video source nodes (`CameraSourceNode`, `VideoFileSourceNode`, `StreamSourceNode`) — they now emit only `VideoFrameData` (port 0) + `SampledData` (port 1, audio); `VideoOutputNode` accepts only `VideoFrameData`. Commits: `63010f3`, `3fc51a3`
  - **`VideoTransformNode` removed** — superseded by `VideoEffectNode`, which covers all of its operations (incl. blur/OpenCV). `VideoTransformOps.{h,cpp}` + `OpenCVTransforms.cpp` stay (used by `VideoEffectOps`). Commit: `688c899`
  - **`ImageData` type deleted** — the only frame type is `VideoFrameData`; grep `ImageData` in `src/` and `tests/` → 0. Commit: `817002e`
  - **Saved-graph consequences:** old graphs with the `"VideoTransform"` registry key or image edges no longer load — rewire to `VideoEffect` + `VideoFrameData` chains (documented in the README)
- **ChatGraphModel.h** moved from `node_editor_ide/` to `BuiltInNodes/Library/types/` (shared library) for generality
- **Documentation**:
  - Plugins hub (`docs/plugins/README.md`) + fixed plugin documentation links in INDEX/Architecture (`b5c204f`)
  - Demo plugin README — documented video nodes and optional OpenCV (`e2c4925`)
  - REQ-SW-PL-018/PL-019 documentation refs backfill; plugin version alignment 0.3.0 → 0.2.0 in `project()` for `demo_nodeditor_nodes` and `node_editor_ide` (inert metadata, matching runtime 0.2.0) (`ed8b334`)
- **StreamUrlValidator** — extracted from `StreamSourceNode` as a standalone header-only helper (`3048fbd`) for unit-testability (http/https/rtsp stream URL validation)
- **Process**: mandatory branch-per-work-item clause in AGENTS.md (`bee28c4`); trunk-based-lite from master (`547401c`); master consolidation (PR #21 `3c8c47f` merged, phase3 branch deleted, PL-020 rebased onto master)
- **Node drop shadows disabled (perf)** — `nodeeditor` default `DefaultStyle.json` ships `ShadowEnabled=false` and display nodes (`VideoOutputNode`, `DaqDisplayNode`, `NumberDisplayDataModel`, `QDevIoDisplayModelObsolete` incl. AudioDisplay) force it per-node. `QGraphicsDropShadowEffect` blur runs per scene repaint and cost ~46% CPU during video playback (Qt6 36% → 17.6% measured).
- **GL blit is now the DEFAULT on Qt5 (REQ-SW-PL-021)** — `VideoOutputNode` selects the display backend: Qt5 = GL blit ON by default (fastest path), Qt6 = native `QVideoWidget` (GL blit OFF). `DAQSTER_GL_BLIT` is a startup-only debug override: `=0` forces the software path (also Qt5), `=1` forces GL (also Qt6). Fixed the "`=0` enables it" bug — the value now matters (`qEnvironmentVariableIntValue`).
- **Auto-fallback when GL is unavailable (REQ-SW-PL-021)** — if the GL context cannot be created (`glPlatformAvailable()` pre-probe before constructing the widget + async `QOpenGLWidget::isValid()` check after show) `VideoOutputNode` logs `GL fallback: <reason>` and switches to the software path (Qt5: frame → QImage → QLabel; Qt6: native QVideoWidget). Video keeps displaying at 25 fps with no crash. The session guard `m_glFailed` prevents resurrecting a broken window.
- **"GPU display" UI toggle (REQ-SW-PL-021)** — checkbox in `VideoOutputNode` (Qt5: visible + checked by default; Qt6: hidden because native QVideoWidget is already GPU). Users can toggle it live; it is applied at the next frame with no crash or video loss. Priority: the UI has the final word after startup; the env var only picks the initial state.
- **Qt5 software display path for the video-frame port** — before, Qt5 without GL blit did not display port-0 (video-frame) frames at all; now `frameToImage` → QLabel shows the video (measurable: `DAQSTER_GL_BLIT=0` ≈ 34-36% CPU).
- **Video source port renumbering on Qt5** — see NV12-direct entry above; old Qt5 saved graphs that connected source port 0 (was "image") to an ImageData consumer lose that edge.

### Fixed
- **RGBA (FBO) input orientation bug in the GPU effect processors (REQ-SW-PL-032)** —
  `VideoEffectGLProcessor` and `CustomShaderGLProcessor` sampled bottom-up
  FBO-produced RGBA textures with the standard top-down quad → even-length GPU
  effect chains (2 effects) and a single CustomShaderNode pass displayed
  vertically flipped. Fix: flipped-v quad (`kQuadVerticesFbo`, v' = 1 − v) when
  sampling an RGBA input / pre-pass intermediate, mirroring the display path
  (`VideoGLBlitWidget`). Single effect (YUV input) unchanged.
- **Per-frame texture allocation in the effect path (REQ-SW-PL-032 Issue #7)** —
  new `TexturePool` (`src/plugins/common/GL/TexturePool.{h,cpp}`): `acquire(w,h)`
  reuses a free texture (re-allocates storage on resolution change),
  `release(tex)` returns it to the pool; `VideoFrameData::fromTexture()` accepts
  an optional release callback (shared_ptr to the pool) that returns the texture
  on frame destruction instead of `glDeleteTextures`. The effect path no longer
  does per-frame `glGenTextures`/`glDeleteTextures`.
- **`setValidationState` re-triggered the full path every frame (nodeeditor
  submodule)** — early-return when the state is unchanged (NumberDisplay-class
  nodes call `setValidationState` every frame with the same state).
- **Double `nodeUpdated` emission (nodeeditor submodule)** — for
  `NodeRole::ValidationState` via the graph model API, `setValidationState` →
  `requestNodeUpdate` → `nodeUpdated` plus a direct `nodeUpdated` emission; the
  direct emission was removed.
- **Write-only `m_videoFrame` in `VideoOutputNode`** — removed (replaced with
  `m_lastInput`, used by `reprocessCurrentFrame()`); `load()` now calls
  `reprocessCurrentFrame()` after loading (consistent with `VideoEffectNode`).
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
- **`[PERF]` console line never printed (REQ-SW-PL-027)** — `VideoOutputNode::logPerfLine()` logged via `qCDebug(lcPerf)`, but the `daqster.perf` category is disabled by default in `LogManager` → the line was filtered out. Switched to `qInfo().noquote()` (Info level, no category — always emitted, like FFmpeg `[INF]` lines); the line format (formatPerfLine) is unchanged
- **Perf overlay badge not visible (REQ-SW-PL-027, Qt6)** — the badge was a `QLabel` child of the detached `QVideoWidget`; `QVideoWidget` renders the video in its own native layer (RHI swapchain) and does NOT composite child widgets on top of it → the badge stayed invisible. Migrated to a separate top-level frameless tool window (`Qt::Tool | FramelessWindowHint | WindowStaysOnTopHint`, `WA_TransparentForMouseEvents`, semi-transparent background) that tracks the video window position (top-left, ~4,4 offset) at creation and on every ~500 ms refresh; `show()`/`hide()` follow `domain.enabled()`; closed on `inputConnectionDeleted`/destructor
- **Detached GL blit window did not reappear on re-connect (Qt5 + Qt6 image port)** — disconnecting hid the GL window (via `updateDisplay()`'s null branch) but `ensureGlWidget()` returned early on the existing-but-hidden widget, and Qt5 `inputConnectionDeleted()` was a no-op → the window never came back on re-connect. `ensureGlWidget()` now re-shows a hidden widget and Qt5 disconnects tear the window down like Qt6. Verified manually on both Qt versions.
- **Qt5 video throttled to 1 fps when the GL window was visible** — on NVIDIA GLX the default swap interval throttles `QOpenGLWidget` repaints to ~1 Hz (reproduced with a minimal standalone widget), which starved the video pipeline to 1 fps. `VideoGLBlitWidget` now sets `QSurfaceFormat::setSwapInterval(0)` (+ default format) so presents are free-running → 25 fps.

### Known issues
- **VC-1 Advanced video on Qt6 (known issue)** — VC-1 Advanced content (e.g. `50MB_1080P_THETESTDATA.COM_AVI.avi`, ASF container mislabelled .avi) shows a green screen on Qt6: the Qt FFmpeg backend (QFFmpegMediaPlugin, libavcodec 61/FFmpeg 7.1) delivers NV12 frames with an ENTIRELY zero chroma plane (Cb=Cr=0) → YCbCr→RGB = solid green (0,226,0). Proven with a standalone probe containing no Daqster presentation code (system FFmpeg decodes correctly; H.264 and Cinepak work on Qt6; Qt5 works via GStreamer). Not fixable in Daqster code — upstream Qt 6.9.2 bug. Discovered 2026-08-09.

### Removed
- **7 deprecated VideoEffect alias nodes (REQ-SW-PL-028, 2026-08-26, user decision)** — `VideoEffectBrightnessNode`, `VideoEffectContrastNode`, `VideoEffectGrayscaleNode`, `VideoEffectInvertNode`, `VideoEffectSepiaNode`, `VideoEffectChannelSwapNode`, `VideoEffectFlipNode` removed from `VideoEffectNode.h` and their registrations from `DemoNodeEditorNodesObject.cpp`. The single `VideoEffect` node (effect combo) is the only registered effect node. **Old saved graphs referencing the alias registry keys no longer load** (accepted consequence).
- **`VideoTransformNode` (REQ-SW-PL-032 Phase 3, 2026-08-26)** — removed from registration, CMake and the filesystem; superseded by `VideoEffectNode` (covers all its operations incl. blur/OpenCV). `VideoTransformOps.{h,cpp}` + `OpenCVTransforms.cpp` stay (used by `VideoEffectOps`). Commit: `688c899`
- **`ImageData` type (REQ-SW-PL-032 Phase 3, 2026-08-26)** — `src/plugins/common/NodeDataTypes/ImageData.h` deleted; the only frame type is `VideoFrameData`. Commit: `817002e`
- **Image ports (REQ-SW-PL-032 Phase 3, 2026-08-26)** — removed from the video source nodes and `VideoOutputNode`; sources emit only `VideoFrameData` + `SampledData` (audio). Commits: `63010f3`, `3fc51a3`

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


