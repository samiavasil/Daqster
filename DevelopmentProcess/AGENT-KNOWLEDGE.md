# AGENT-KNOWLEDGE.md — Architecture Knowledge (Архитектурно знание)

Този файл възстановява **архитектурния контекст** на проекта за нова сесия:
топология, типове данни, дизайнерски решения и правила.

---

## 1. Plugin Topology (Топология на плъгините)

| Репо | Път | Роля |
|------|-----|------|
| **daqster** (публично) | `./` | Daqster framework + публични плъгини + общи NodeDataTypes |
| **DaqsterAiStudio** (частно) | `../DaqsterAiStudio/` | Частни плъгини (ai_studio_plugin) |

- CMake модулите (`FindQtVersion`, `PluginDependencyManager`, `ComponentTemplates`)
  се зареждат от `cmake/`.

## 2. Node Data Type Inventory (Инвентар на NodeData типовете)

Публични (`src/plugins/common/NodeDataTypes/`):

| Тип | ID / Display | Payload | Бележка |
|-----|--------------|---------|---------|
| `TextData` | `"text"` / `"Text"` | `QString` | |
| `EmbeddingData` | `"embedding"` / `"Embedding"` | `std::vector<float>` + `dim()` | **НЯМА consumer node** |
| ~~`ImageData`~~ | ~~`"image"`~~ | ~~`QImage`~~ | **ПРЕМАХНАТ (2026-08-26)** — единственият frame тип е `VideoFrameData` |
| `VideoFrameData` | `"video-frame"` / `"Video Frame"` | `QVideoFrame` | Native buffer (GPU/zero-copy display); **единственият frame тип** — lazy `QImage` кеш `asImage()` + lazy GL текстура кеш `asTexture()`/`fromTexture()`/`isGpuResident()` |
| `SampledData` | `"sample"` / `"Sample"` | `QByteArray` + `SampledStreamDescriptor` | DAQ/аудио семпли (REQ-SW-PL-022); `decodeToNormalized()`/`decodeToPhysical()` |
| `FloatData` | `"float"` / `"Float"` | `float` | |
| `NumericType` | RTTI-based | int/double | Built-in нодове |

## 3. Zero-Copy Design Decision (Дизайн: нулево копиране)

- **Между нодове = `shared_ptr<NodeData>` propagation** — копира се shared pointer,
  не буферът.
- **QImage implicit sharing:** преди мутация винаги `detach()`.
- **VideoFrameData lazy кешове:** `asImage()` и `asTexture()` конвертират
  **най-много веднъж на кадър** и кешират резултата; `setFrame()` инвалидира
  кешовете. GUI-thread only (без mutex).

## 4. Video Nodes (Видео нодове, публичен demo plugin)

- Категория `"Video"` в `demo_nodeditor_nodes` (`Sources/Video/`):
  `CameraSourceNode`, `VideoFileSourceNode`, `StreamSourceNode`, `VideoOutputNode`,
  `VideoEffectNode` (един нод с комбо, REQ-SW-PL-028), `CustomShaderNode`
  (REQ-SW-PL-029), `FrameSamplerNode` (REQ-SW-PL-030).
  **`VideoTransformNode` е ПРЕМАХНАТ (2026-08-26)** — заменен от `VideoEffectNode`;
  `VideoTransformOps.{h,cpp}` + `OpenCVTransforms.cpp` остават (ползвани от
  `VideoEffectOps`).
- **`VideoCompat.h` = Qt5/Qt6 multimedia shim** — namespace от type aliases и
  inline helpers. **ВСИЧКИ multimedia версионни разлики отиват ТАМ.**
- Qt6 camera routing изисква `QMediaCaptureSession`.

## 4a. CustomShaderNode (REQ-SW-PL-029)

- **GPU compute нод** с runtime GLSL (data → текстура → шейдър → текстура →
  data); v1 hardcoded към `VideoFrameData` (port 0 in/out) — **няма pluggable
  `IDataAdapter` в кода**; v2+ SampledData/DAQ, TensorData остават бъдещ лост.
- **`CustomShaderGLProcessor`** — runtime GLSL compile (`addShaderFromSourceCode`),
  per-(effect,layout,profile) program cache, YUV→RGBA pre-pass, error handling
  без crash.
- **MainImage contract** — шейдърът трябва да дефинира `void mainImage(out vec4 fragColor, in vec2 fragCoord)`.
- **texture() compat fix** (`0682c1b`) — GL profile detection и използване на
  `texture()` (GLSL 130+) или `texture2D()` (GLSL 120) според GLSL version.
- **UI:** GLSL редактор + compile button + error log + uniform controls
  (строи се в `CustomShaderNode::buildWidget()`, `CustomShaderNode.cpp:117-188`).

## 4b. Video Frame Consolidation (Фаза 3 завършена 2026-08-26)

- **Един тип `VideoFrameData`:** `QVideoFrame` + lazy `QImage` кеш `asImage()` +
  lazy GL текстура кеш. Display = `frame()` zero-copy; processing = `asImage()`
  1×/кадър споделено.
- **GPU-resident ефект верига (Stage 2A/2B):** `asTexture()` (lazy upload) →
  `processTexture()` (bind, без upload/readback) → `fromTexture()` (RGBA текстура) →
  GL blit display (`presentTexture`, zero-copy).
- **Scene invalidation fix (2026-08-27):** `onNodeDataArrived` +
  `dataArrivalChangesGeometry` в nodeeditor; `CustomDataFlowScene` override +
  3 video nodes opt-out (repaint-only).
- **VideoFrameData::fromTexture()** — wrap-ва GPU-resident RGBA текстура
  (ефект изход).
- **VideoGLContextManager** — process-wide споделен `QOpenGLContext` +
  `QOffscreenSurface` (share group с `QOpenGLWidget` display-а).

## 4c. Sample Port Fix (REQ-SW-PL-022, commit `d5145c2`)

- `VideoFileSourceNode` и `StreamSourceNode` имаха nPorts=2 вместо 3 — audio
  Sample port на индекс 2 беше недостъпен. Поправен: nPorts=3 (video-frame@0,
  image@1, sample@2).

## 5. OpenCV (опционален)

- `find_package(OpenCV QUIET)` + `option(DAQSTER_USE_OPENCV ... ON)` + `HAVE_OPENCV`.
- Plugin-ът ВИНАГИ се build-ва без OpenCV — 8 базови QImage операции fallback.

## 6. RDD Gate (RDD процес)

- Lifecycle: `discuss → decide → create → implement → verify → update status`.
- **STANDING (до ново нареждане):** имплементации **без unit тестове** →
  имплементираните изисквания остават **ACTIVE** с отложени тестове; `DONE`
  изисква пълна верификация (Qt5 + Qt6 builds + unit тестове + headless smoke).

## 7. Plugin Pattern (Патърнът на плъгините)

- **DemoStandardNodes 3 слоя:** Interface (`QPluginInterface`, factory + metadata)
  → Object (`QBasePluginObject` + `INodeProvider`, lifecycle + `registerNodes()`)
  → Node модели (`NodeDelegateModel`, тънък controller → Engine + Widget).
- CMake: `create_plugin()` макро (от `cmake/ComponentTemplates.cmake`);
  **НЕ ползвай `daqster_add_plugin()`** (не съществува).
- Изходният `.so` filename **ТРЯБВА да съдържа "plugin"** — `QPluginManager`
  филтрира по този substring.

## 8. Where Docs Live (Къде живее документацията)

| Документ | Език |
|----------|------|
| `docs/plugins/<plugin>/README.md` (demo_nodeditor_nodes, node_editor_ide, QtCoinTrader) | Български |
| `docs/plugins/README.md` | Български |
| `docs/Architecture/` | Български (+ някои `.en.md`) |
