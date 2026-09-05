# Demo NodeEditor Nodes Plugin

Родител: [Plugins](../README.md) | [Architecture](../../Architecture/README.md)

## Преглед

INodeProvider плъгин, който доставя Audio, Video, LLaMA, Display и Routing нодове към Node Editor IDE. Това е основният доставчик на нодове за Daqster node editor-а.

**Локация:** `src/plugins/demo_nodeditor_nodes/`
**Име на .so:** `libDemoNodeEditorNodesPlugin.so`

## Архитектура

```
demo_nodeditor_nodes/
├── DemoNodeEditorNodesInterface.{h,cpp}  # QPluginInterface — factory
├── DemoNodeEditorNodesInterface.json     # Plugin metadata
├── DemoNodeEditorNodesObject.{h,cpp}     # QBasePluginObject + INodeProvider
├── Sources/
│   ├── AudioSource/                      # Аудио входни нодове (SampledData + obsolete QDevIO)
│   │   ├── AudioSourceDataModel.{h,cpp}  # SampledData mic — worker-thread capture (REQ-SW-PL-024)
│   │   ├── MicCaptureWorker.{h,cpp}      # Capture worker (moveToThread, QAudioSource + SampledData)
│   │   ├── AudioSourceDataModelObsolete.{h,cpp}  # Старият QDevIO mic (rename-only, работи)
│   │   ├── AudioWorkerObsolete.{h,cpp}   # Старият QDevIO capture worker
│   │   ├── AudioNodeQdevIoConnectorObsolete.{h,cpp}  # Старият QDevIO connector
│   │   ├── AudioSourceDataModelUI.{h,cpp}
│   │   ├── AudioSourceConfig.{h,cpp,ui}
│   │   ├── AudioComboModel.{h,cpp}
│   │   └── node_editor.qrc
│   ├── LLamaSource/                      # LLaMA AI нодове
│   │   ├── LLamaModelDataModel.{h,cpp}
│   │   ├── ConsoleDataModel.{h,cpp}
│   │   └── ChatBaseWidget.{h,cpp}
│   └── Video/                            # Video нодове (VideoFrameData / "video-frame" flow)
│       ├── VideoCompat.h                 # Qt5/Qt6 multimedia compatibility shim
│       ├── CameraSourceNode.{h,cpp}
│       ├── VideoFileSourceNode.{h,cpp}
│       ├── StreamSourceNode.{h,cpp}
│       ├── StreamUrlValidator.{h,cpp}    # Stream URL валидация (http/https/rtsp)
│       ├── AudioBufferToSampled.h        # Audio buffer → SampledData glue (REQ-SW-PL-024)
│       ├── VideoOutputNode.{h,cpp}
│       ├── VideoGLBlitWidget.{h,cpp}     # Detached GL blit display widget (DAQSTER_GL_BLIT)
│       ├── VideoPerfBadge.{h,cpp}        # Perf badge/line форматър (REQ-SW-PL-027)
│       ├── VideoTransformOps.{h,cpp}     # Op engine (QImage + params → QImage)
│       ├── VideoGLShaders.h              # Споделени GLSL source builder-и (blit + effect)
│       ├── VideoEffectOps.{h,cpp}        # EffectSpec регистър (ефекти, REQ-SW-PL-028)
│       ├── VideoEffectGLProcessor.{h,cpp}# GPU backend (offscreen FBO, REQ-SW-PL-028)
│       ├── VideoEffectNode.{h,cpp}       # Един VideoEffect нод с комбо (REQ-SW-PL-028)
│       ├── CustomShaderGLProcessor.{h,cpp}# GPU backend за runtime GLSL (REQ-SW-PL-029)
│       ├── CustomShaderNode.{h,cpp}      # Custom shader нод (REQ-SW-PL-029)
│       ├── FrameSamplerNode.{h,cpp}      # Ресемплер (every-N / max-fps, REQ-SW-PL-030)
│       └── OpenCVTransforms.cpp          # Само при HAVE_OPENCV
├── Displays/
│   ├── AudioDisplay/
│   │   └── AudioDisplayModelObsolete.{h,cpp}  # Старият QDevIO audio display (rename-only)
│   ├── DaqDisplay/
│   │   └── DaqDisplayNode.{h,cpp}        # Реален Qt Charts waveform + FFT (REQ-SW-PL-022/025)
│   └── GenericDisplay/
│       └── GenericDisplayNode.{h,cpp}
└── Routing/
    ├── Demux/
    │   └── DemuxNodeObsolete.{h,cpp}     # Старият QDevIO demux (rename-only)
    └── Mux/
        └── MuxNodeObsolete.{h,cpp}       # Старият QDevIO mux (rename-only)
```

## Имплементирани интерфейси

### QPluginInterface ( frame_work )
```
DemoNodeEditorNodesInterface → QPluginInterface
  ├── Q_PLUGIN_METADATA(IID "Daqster.PlugIn.QPluginInterface")
  ├── CreatePluginInternal() → създава DemoNodeEditorNodesObject
  └── m_PluginDescryptor:
        PLUGIN_NAME       = "DemoNodeEditorNodes"
        PLUGIN_TYPE       = APPLICATION_PLUGIN
        PLUGIN_TYPE_NAME  = "Node Providers"   ← групиране в PluginManager GUI
        PLUGIN_VERSION    = "0.3.2"
```

### QBasePluginObject ( frame_work )
```
DemoNodeEditorNodesObject → QBasePluginObject
  ├── Initialize()   → return true (няма GUI — само доставя нодове)
  └── DeInitialize() → логира унищожаване
```

### INodeProvider ( standalone interface )
```
DemoNodeEditorNodesObject → INodeProvider
  └── registerNodes(registry) →   // 21 регистрации (DemoNodeEditorNodesObject.cpp:88-128)
        // Displays (6)
        registry.registerModel<AudioDisplayModelObsolete>("Obsolete")
        registry.registerModel<AudioDisplayAlias>("Daq/Display") // old key "AudioDisplay" -> SampledData display
        registry.registerModel<GenericDisplayNode>("Daq/Display")
        registry.registerModel<DaqDisplayNode>("Daq/Display")
        registry.registerModel<QDevIoDisplayModelObsolete>("Obsolete")
        registry.registerModel<QDevIoDisplayModelObsoleteAlias>("Obsolete") // old key "QDevIoDisplay"
        // Routing (4)
        registry.registerModel<DemuxNodeObsolete>("Obsolete")
        registry.registerModel<DemuxNodeObsoleteAlias>("Obsolete") // old key "DemuxNode"
        registry.registerModel<MuxNodeObsolete>("Obsolete")
        registry.registerModel<MuxNodeObsoleteAlias>("Obsolete") // old key "MuxNode"
        // Sources (2)
        registry.registerModel<AudioSourceDataModel>("Audio/Sources")
        registry.registerModel<AudioSourceDataModelObsolete>("Obsolete")
        // LLama (2)
        registry.registerModel<LLamaModelDataModel>("AI/LLM")
        registry.registerModel<ConsoleDataModel>("General/Display")
        // Video (7)
        registry.registerModel<CameraSourceNode>("Video/Sources")
        registry.registerModel<VideoFileSourceNode>("Video/Sources")
        registry.registerModel<StreamSourceNode>("Video/Sources")
        registry.registerModel<VideoOutputNode>("Video/Display")
        registry.registerModel<VideoEffectNode>("Video/Processing")   // един нод с комбо (REQ-SW-PL-028 AC 4)
        registry.registerModel<CustomShaderNode>("Video/Processing") // runtime GLSL (REQ-SW-PL-029)
        registry.registerModel<FrameSamplerNode>("Video/Processing")
```

**INodeProvider е standalone интерфейс** — не наследява други Daqster интерфейси.
Открива се от `node_editor_ide` чрез `QPluginManager::instances(INodeProvider_IID)`.

## Доставени нодове

### Sources
| Нод | Категория | Описание |
|-----|-----------|----------|
| AudioSourceDataModel | Sources | Аудио вход (микрофон) — SampledData поток `{"sample","Sample"}`; каптурата е в dedicated worker thread, GUI нишката само пази последния `shared_ptr` и емитира `dataUpdated` |
| AudioSourceDataModelObsolete | Sources | Старият QDevIO аудио вход (rename-only, работи) — излъчва QDevIO byte-stream за legacy графите и benchmarking |

### Displays
| Нод | Категория | Описание |
|-----|-----------|----------|
| DaqDisplayNode | Daq/Display | **Каноничният SampledData дисплей** — реален Qt Charts waveform + FFT за всеки плъгин с sampled данни (audio/DAQ/сензори). v2 (REQ-SW-PL-025): физически decode (`decodeToPhysical`, `raw × amplitudeScale + amplitudeOffset`), unit оси от дескриптора (Time (s)/Hz + мерна единица), worker-притежаван N-секунден ring buffer (default 10 s) с FFT от опашката; per-card `mode` (normalized/physical) + `unitAxes`, backward-compatible save/restore |
| GenericDisplayNode | Daq/Display | Тънък alias на DaqDisplayNode (само name/caption) — legacy key "GenericDisplay" |
| AudioDisplayAlias | Daq/Display | Тънък alias на DaqDisplayNode (само name) — legacy key "AudioDisplay", консолидиран върху SampledData display-а |
| AudioDisplayModelObsolete | Obsolete | Старият QDevIO аудио дисплей (rename-only, registered `AudioDisplayObsolete`) — за стари QDevIO графи |
| QDevIoDisplayModelObsolete | Obsolete | Старият QDevIO display (rename-only, registered `QDevIoDisplayObsolete` + alias `QDevIoDisplay`) |

### Routing
| Нод | Категория | Описание |
|-----|-----------|----------|
| DemuxNodeObsolete | Routing | Старият QDevIO demultiplexer (rename-only, registered `DemuxNodeObsolete` + alias `DemuxNode`) |
| MuxNodeObsolete | Routing | Старият QDevIO multiplexer (rename-only, registered `MuxNodeObsolete` + alias `MuxNode`) |

### LLama
| Нод | Категория | Описание |
|-----|-----------|----------|
| LLamaModelDataModel | LLama | LLaMA модел — AI генерация на текст |
| ConsoleDataModel | LLama | Конзола за показване на AI отговори |

### Video
| Нод | Категория | Описание |
|-----|-----------|----------|
| CameraSourceNode | Video | Заснема кадри от локално camera устройство (избор на устройство + start/stop), емитира кадри — port 0 `VideoFrameData` (Qt6: zero-copy; Qt5: OWNED copy) |
| VideoFileSourceNode | Video | Възпроизвежда локален видео файл през `QMediaPlayer` + frame probe (browse + play/pause), емитира кадри — port 0 `VideoFrameData` (Qt6: zero-copy; Qt5: OWNED copy) + port 1 `SampledData` (audio) |
| StreamSourceNode | Video | Възпроизвежда HTTP/RTSP stream (URL поле + connect), емитира кадри — port 0 `VideoFrameData` (Qt6: zero-copy; Qt5: OWNED copy) + port 1 `SampledData` (audio) |
| VideoOutputNode | Video | Live preview на входящите кадри — един вход (port 0 `VideoFrameData` → GPU display), zero-copy GPU път. Qt6: чекбокс „GPU display" (checked по подразбиране) → detached прозорец (`QVideoWidget` или GL blit при `DAQSTER_GL_BLIT=1`); unchecked → in-scene `QGraphicsVideoItem` в node-а (REQ-SW-PL-021). Qt5: checked → detached GL blit прозорец, unchecked → софтуерен QLabel. Pass-through изходен порт за output вериги. **Опционален вграден ефект комбо** (REQ-SW-PL-034): „No effect" по подразбиране + 11-те ефекта на `VideoEffectNode` (GPU/CPU по backend); при избран ефект GpuRgba кадрите се презентират с GL blit на Qt6 (Stage 2C, zero-copy `presentTexture`), а `presentYuvTexture` reuse-ва кешираните YUV текстури |
| VideoEffectNode | Video | Единен ефект нод с **комбобокс** за избор на ефект (brightness, contrast, grayscale, invert, sepia, channelSwap, flip, blur, gaussianBlur, canny, threshold) + параметри за избрания — върху `VideoFrameData`, GPU/CPU backend (REQ-SW-PL-028) |
| CustomShaderNode | Video | GPU-only runtime GLSL нод — Shadertoy-style `mainImage` contract, GLSL редактор + compile + error log + uniform контроли, port 0 in/out `VideoFrameData`, изисква хардуерен GL (REQ-SW-PL-029) |
| FrameSamplerNode | Video | Ресемплиране на `VideoFrameData` — всеки N-ти кадър или max FPS, zero-copy passthrough (REQ-SW-PL-030) |

Всички Video нодове обменят данни от публичните shared NodeDataTypes (REQ-SW-PL-013). Източниковите нодове (`CameraSourceNode`, `VideoFileSourceNode`, `StreamSourceNode`) имат port 0 `VideoFrameData` ("video-frame", zero-copy); видео source-ите имат и port 1 `SampledData` (audio, appended last — REQ-SW-PL-022 AC 8). `VideoOutputNode` приема `VideoFrameData` и го дисплейва през GPU (вж. „Qt6 in-scene toggle" по-долу; Qt5: detached GL blit прозорец при `DAQSTER_GL_BLIT=1`). **Единственият frame тип е `VideoFrameData`** — `ImageData` е премахнат (Фаза 3, REQ-SW-PL-032); CPU обработката ползва lazy `VideoFrameData::asImage()` кеша. Частният AI Studio plugin консумира `VideoFrameData` на входа на `FrameToTensorNode` (REQ-AI-007).

#### Qt6 in-scene GPU display toggle (REQ-SW-PL-021)

`VideoOutputNode` на Qt6 има видим чекбокс **„GPU display"** (checked по
подразбиране — запазва текущото detached поведение):

- **Checked (detached, default):** видеото се показва в отделен прозорец —
  native `QVideoWidget` (GPU, RHI swapchain) или GL blit прозорец при
  `DAQSTER_GL_BLIT=1` (debug override за стартиране).
- **Unchecked (in-scene):** видеото се рендерира **вътре в node-а** — в сцената
  се създава `QGraphicsVideoItem` като child на `NodeGraphicsObject`-а на node-а
  (позиция/размер = label area-та). Кадрите се подават през
  `VideoCompat::presentFrame(item->videoSink(), frame)` — GPU път без QImage
  копие (при SW decode Qt рендерира кадрите без QImage копие в приложението).
  Софтуерният `QLabel` път остава като automatic fallback.
- При toggle действието се прилага при следващия кадър — без crash, без загуба
  на видео; detached прозорците се затварят веднага при unchecked, in-scene
  item-ът се трие при checked/дисконект.
- **Headless dev driver:** `DAQSTER_SCENE_VIDEO=1` + `DAQSTER_AUTOSTART_VIDEO=1`
  автоматично uncheck-ва чекбокса (in-scene режим) за безглава проверка.
- Qt5 пътищата са непроменени: checked → detached GL blit, unchecked → софтуерен
  QLabel.

> **Преномерация (NV12-direct, 2026-08-13):** на Qt5 източниковите нодове и
> `VideoOutputNode` вече имат същата топология като Qt6 — port 0 е
> "video-frame", а image портът беше port 1 (беше port 0 на Qt5). Стари saved Qt5
> графи, които свързват source port 0 (беше "image") с ImageData консуматор,
> **загубиха връзката** и трябваше да се пресвържат към новия image порт (1).
> Причина: Qt5 сега транспортира OWNED NV12/YUV420P копия (`VideoCompat::frameToOwnedFrame`),
> така че `VideoFrameData` не е вече Qt6-only.
>
> **Фаза 3 (2026-08-26, REQ-SW-PL-032):** image портът е **премахнат изцяло** —
> източниковите нодове емитират само `VideoFrameData` (port 0) + `SampledData`
> (port 1, audio), `VideoOutputNode` приема само `VideoFrameData`, а типът
> `ImageData` е изтрит. **Saved-graph последица:** стари графи, които свързват
> image порт (port 1 на source / port 1 на output) с `ImageData` консуматори
> (напр. `VideoTransformNode`), **няма да се заредят** — `VideoTransformNode` е
> премахнат (заменен от `VideoEffectNode`), image edges-ите отпадат (type
> mismatch), и графите трябва да се пресвържат към `VideoFrameData` вериги
> (source → `VideoEffectNode` → output).

#### VideoTransformNode — премахнат (REQ-SW-PL-032, Фаза 3)

`VideoTransformNode` (REQ-SW-PL-019) беше **премахнат на 2026-08-26** — заменен
от `VideoEffectNode` (REQ-SW-PL-028), който покрива всичките му операции върху
`VideoFrameData`. Операционният engine (`VideoTransformOps.{h,cpp}`) и OpenCV
функциите (`OpenCVTransforms.cpp`) **остават** — `VideoEffectOps` ги ползва за
CPU ефектите. Стари saved графи с `"VideoTransform"` registry ключ **няма да се
заредят**; пресвържете ги към `VideoEffect` (комбо = същата операция).

#### VideoEffectNode — един нод с комбобокс, GPU/CPU backend (REQ-SW-PL-028)

`VideoEffectNode` е видео ефект нод с **runtime-избран backend по ефект**.
Ефектът се избира от **комбобокс** (комбо + `QStackedWidget`) — **един нод
тип**, не отделни subclass-а (решение 2026-08-25, REQ-SW-PL-028 AC 4). Работи
върху `VideoFrameData` (port 0 in / port 0 out) — не тригерира lazy `asImage()`
освен при CPU обработка.

- **`EffectSpec`** (`VideoEffectOps.{h,cpp}`) — описание на ефекта: id,
  displayName, backend (`CpuOnly` / `GpuOrCpu`), CPU функция (делегира на
  `VideoTransformOps`) и опционален GLSL body за GPU backend-а. EffectSpec-ите
  остават — комбобоксът избира между тях (REQ-SW-PL-028 AC 5).
- **Ефекти (11):** brightness, contrast, grayscale, invert, sepia, channelSwap,
  flip, **blur** (box blur, radius 0..10), **gaussianBlur** (OpenCV, kernel
  1..31), **canny** (OpenCV, low/high 0..255), **threshold** (OpenCV, value
  0..255) — последните три са `CpuOnly` и налични само при `HAVE_OPENCV`
  (REQ-SW-PL-028, добавени 2026-08-26). Това покрива всички операции на
  премахнатия `VideoTransformNode`.
- **UI:** комбо с displayName-ите на ефектите + суфикс за backend-а
  ((GPU)/(CPU), по `EffectSpec::Backend`) + `QStackedWidget` с по една
  страница параметри за ефект (brightness slider −100..+100, contrast slider
  0..200%, blur radius slider, gaussian kernel slider, canny/threshold sliders,
  flip combo horizontal/vertical, info label за grayscale/invert/
  sepia/channelSwap). `setEffect(id)` избира по id (непознат id → индекс 0);
  `save()`/`load()` персистират `"effect"` = id + параметрите
  с clamp-ове — форматът е **backward compatible** със старите графи.
- **Deprecated aliases (премахнати 2026-08-26):** 7-те alias нода
  (`VideoEffectBrightnessNode`, `VideoEffectContrastNode`,
  `VideoEffectGrayscaleNode`, `VideoEffectInvertNode`, `VideoEffectSepiaNode`,
  `VideoEffectChannelSwapNode`, `VideoEffectFlipNode`) бяха **премахнати** по
  решение на потребителя — стари saved графи, които реферират тези registry
  ключове, **вече няма да се зареждат**; единственият регистриран ефект нод е
  `VideoEffect` (с комбобокс).
- **Backend избор (runtime):** `CpuOnly` ефекти вървят на CPU навсякъде;
  `GpuOrCpu` ефекти вървят на GPU когато има хардуерен GL и падат на CPU
  иначе (включително при не-NV12/YUV420P формат на кадъра).
- **`VideoEffectGLProcessor`** — GPU backend: собствен `QOpenGLContext` +
  `QOffscreenSurface`, компилира `buildVertexSource` +
  `buildEffectFragmentSource` (от `VideoGLShaders.h`), upload-ва Y/U/V
  plane-овете с `GL_UNPACK_ROW_LENGTH = bytesPerLine`, рендерира в
  `QOpenGLFramebufferObject` с размера на кадъра и чете резултата с
  `toImage()` (вграден вертикален флип). `hasHardwareGL()` различава
  хардуерен GL от `llvmpipe`/`softpipe`/`SwiftShader` (lazy кеширано).
- **RGBA input orientation (2026-08-28, REQ-SW-PL-032):** FBO-произведените
  RGBA текстури (изход на предишен ефект) са bottom-up — processor-ът ги
  семплира с **flipped-v quad** (`kQuadVerticesFbo`, v' = 1 − v) при
  `input.rgba`, огледално на display path-а (`VideoGLBlitWidget`). Без това
  even-length GPU ефект вериги (2 ефекта) се показваха вертикално обърнати;
  single effect (YUV input, top-down) остава непроменен.
- **Texture pool (2026-08-28, REQ-SW-PL-032 Issue #7):** изходните RGBA
  текстури идват от `TexturePool` (`src/plugins/common/GL/TexturePool.{h,cpp}`)
  — `acquire(w,h)` reuse-ва свободна текстура (при смяна на резолюцията
  преоразмерява storage-а на същия texture name), `release(tex)` я връща в
  pool-а. `VideoFrameData::fromTexture()` приема опционален release callback
  (shared_ptr към pool-а), който се вика при унищожаване на frame-а вместо
  `glDeleteTextures` — **без per-frame glGenTextures/glDeleteTextures** в
  ефект пътя. Без callback поведението остава delete-по подразбиране.
- **`CustomShaderGLProcessor`** — същият orientation fix: custom pass-ът
  семплира bottom-up RGBA (pre-pass intermediate или RGBA input) с flipped-v
  quad, така че единичен CustomShaderNode pass стои прав; YUV→RGBA pre-pass
  семплира top-down YUV input със стандартния quad. Изходните текстури също
  идват от `TexturePool`.
- **GLSL ефекти:** brightness (`rgb + u_brightness`), contrast
  (`(rgb − 0.5) * u_contrast + 0.5`), grayscale (dot luminance), invert,
  sepia (mat3 multiply), channelSwap (`rgb.bgr`), flip (празен body — флипът
  става през **двата** uniform-а `u_flipX` (хоризонтален) и `u_flipY`
  (вертикален), които обръщат texture coordinate-а — `VideoEffectGLProcessor.cpp:204-209`;
  CPU пътят ползва horizontal/vertical combo).
- **Lazy QImage кеш (REQ-SW-PL-032):** CPU пътят ползва `VideoFrameData::asImage()`
  — конверсията става най-много веднъж на кадър и се споделя между всички
  CPU консуматори (fan-out).
- **Smoke driver:** `DAQSTER_AUTOSTART_EFFECT=<effectId>` добавя един
  `VideoEffect` нод и задава ефекта през `load()` (същият път като saved
  граф) между source и output в `autoStartVideo()`.

#### FrameSamplerNode — ресемплиране (REQ-SW-PL-030)

`FrameSamplerNode` е отделен нод за ресемплиране на video кадри
(`VideoFrameData` port 0 in / port 0 out):

- **Режими:** „Every N-th frame" (N = 1..1000) или „Max FPS" (1..120) —
  избираемо от combo + QSpinBox.
- **Zero-copy, fan-out:** при pass нодът предава **същия**
  `shared_ptr<VideoFrameData>` (ref-count bump) — без QImage конверсия, без
  копие на frame-а; ресемплираният кадър стига до всички консуматори.
- **Lazy конверсия:** нодът работи върху frame-а, никога не вика
  `asImage()`/`frameToImage()`.
- **Gate:** EveryNth → `++counter; pass = (N <= 1) || (counter % N == 0)`;
  MaxFps → таймер стартира на първи кадър, `pass = elapsed >= 1e9 / maxFps`,
  при pass `restart()`. При не-pass кадърът се изпуска без emit.
- **Smoke driver:** `DAQSTER_AUTOSTART_SAMPLER=1` вмъква FrameSampler между
  source (или effect) и output в `autoStartVideo()`.

## AudioSource — SampledData миграция (REQ-SW-PL-024)

Mic пътят мигрира от QDevIO byte-stream към SampledData. Нов нод заема името
`AudioSourceDataModel` (registered name `AudioSource`); старият QDevIO mic е
преименуван на `AudioSourceDataModelObsolete` (registered name `AudioSourceObsolete`)
и остава работещ — основата за benchmarking старо vs ново.

### SampledData порт (новият нод)

- Един изходен порт, тип `{"sample","Sample"}` (от `SampledData().type()`).
- `outData()` връща най-новото `shared_ptr<SampledData>` (member `m_lastData`).
- **Няма остатък от QDevIO** в новия нод.

### Threading — dedicated worker thread

- `MicCaptureWorker` QObject (queued slots `startCapture` / `stopCapture` /
  `setCaptureEnabled` / `updateDevice`) е `moveToThread`-нат в модел-притежаван
  `QThread` (`AudioSourceCaptureThread`).
- `QAudioSource`/`QAudioInput` се създава **в worker нишката**; `readyRead` →
  `readAll()` (PCM raw) → `AudioBufferToSampled::descriptorFromFormat` + wrap в
  `shared_ptr<SampledData>` → queued сигнал `samplesReady` → модела (GUI).
- Ако изходът не е свързан, worker-ът **дренира и не wrap-ва** (`m_connected`
  atomic, сетван през queued `setCaptureEnabled` от `outputConnectionCreated/Deleted`).
- GUI нишката прави само: keep-latest, `dataUpdated(0)`, UI wiring. Няма mutex за
  данните — SampledData се произвежда изцяло в worker нишката и се предава по
  shared_ptr (atomic refcount).
- Stop: queued `stopCapture` → `QAudioSource::stop()`. Деструктор:
  `m_thread->quit(); m_thread->wait();` — worker-ът се изтрива през
  `QThread::finished` → `deleteLater`.

### UI (споделен)

`AudioSourceDataModelUI` (Start/Stop, device/format контроли) е общ за двата нода.
Типът `AudioSourceDataModelUI::StartStop` (ASDM_STOP/ASDM_START/ASDM_RELOAD) е
дефиниран в UI header-а; сигналите `Start` и `ChangeAudioConnection` на новия нод
се маршрутизират към worker-а през queued слотове.

### Obsolete rename таблица

| Старо име | Ново име |
|-----------|----------|
| `AudioSourceDataModel` (QDevIO) | `AudioSourceDataModelObsolete` (registered `AudioSourceObsolete`) |
| `AudioWorker` | `AudioWorkerObsolete` |
| `AudioNodeQdevIoConnector` | `AudioNodeQdevIoConnectorObsolete` |
| `EventThreadPull` (node_editor_ide threading) | `EventThreadPullObsolete` |

### Saved-graph последица (приета)

Стари графи с `"AudioSource"` инстанцират **новия** SampledData нод; QDevIO
edges-ите към AudioDisplay/Mux/Demux отпадат (type mismatch). Стари графи с
`"AudioDisplay"`/`"MuxNode"`/`"DemuxNode"` (QDevIO свят, alias към `_obsolete`
версиите по REQ-SW-PL-023) работят с `AudioSourceObsolete`. Новите графи
свързват `AudioSource` → DAQ Display (SampledData поток).

## Зависимости

Plugin-ът изисква следните Qt модули и библиотеки:
- **Qt::Core, Qt::Gui** — основни Qt модули
- **Qt::Charts** — за графична визуализация
- **Qt::Multimedia** — за аудио и видео функционалност (AudioSource, Video нодове)
- **Qt::Network** — за мрежова комуникация (LLaMA API, StreamSource)
- **Qt::OpenGL** — за rendering
- **QtNodes** — node editor library
- **NodeEditorLibrary** — споделена библиотека за node editor
- **frame_work** — Daqster core framework
- **OpenCV 4.x (ОПЦИОНАЛЕН)** — само за OpenCV video ефектите (GaussianBlur, Canny, Threshold)

### OpenCV (опционален)

`find_package(OpenCV QUIET)` в `CMakeLists.txt` + `option(DAQSTER_USE_OPENCV ... ON)`. Когато `DAQSTER_USE_OPENCV=ON` и OpenCV е открит:
- `OpenCVTransforms.cpp` се компилира и дефинира `HAVE_OPENCV`
- OpenCV ефектите (GaussianBlur, Canny, Threshold) се добавят към комбо-то на `VideoEffectNode` (CpuOnly)
- `${OpenCV_INCLUDE_DIRS}` / `${OpenCV_LIBS}` се подават към `create_plugin()`

Когато OpenCV липсва или `DAQSTER_USE_OPENCV=OFF`, plugin-ът се build-ва нормално с базовите QImage ефекти; OpenCV ефектите отсъстват от комбо-то. Проверено: Qt5 (5.15.2) + Qt6 (6.9.2) builds PASS и с двата варианта (OpenCV 4.6.0 / без OpenCV).

## Qt5/Qt6 съвместимост (VideoCompat.h)

Video нодовете използват Qt Multimedia API, който се различава между Qt5 и Qt6. Разликите са абстрахирани в `Sources/Video/VideoCompat.h` (по модела на съществуващия `AudioCompat.h`) — namespace от type aliases и inline helpers с version-agnostic API за caller-ите:

- **Frame capture:** Qt6 `QVideoSink` (`videoFrameChanged`) vs Qt5 `QVideoProbe` (`videoFrameProbed`) — и двете доставят `QVideoFrame`. Qt6 транспортира кадъра zero-copy; Qt5 прави OWNED копие (`frameToOwnedFrame`, NV12/YUV420P) за безопасно преминаване през графа.
- **Frame transport:** `VideoCompat::frameToFrame()` — Qt6 identity (ref-count bump), Qt5 `frameToOwnedFrame()` (map + memcpy plane-ове в `QAbstractPlanarVideoBuffer`). `pixelFormatInt()` нормализира Qt5/Qt6 pixel format за `[PERF]` маркерите.
- **Camera enumeration:** Qt6 `QMediaDevices::videoInputs()` vs Qt5 `QCameraInfo::availableCameras()`
- **Media source:** Qt6 `QMediaPlayer::setSource(QUrl)` vs Qt5 `setMedia(QMediaContent)`
- **Playback state / error сигнали:** Qt6 `playbackStateChanged(PlaybackState)` / `errorOccurred(Error, QString)` vs Qt5 `stateChanged(State)` / `error(Error)` — транспортират се като int/callback (и двата enum-а споделят едни и същи unscoped стойности)
- Qt6 camera routing изисква `QMediaCaptureSession` (създаден като child на sink-а, за да съвпада lifetime-ът с frame capture-а)

## Вид на плъгина

Това е **HEADLESS** плъгин — няма GUI. `Initialize()` връща `true` без да създава прозорец.
Целта му е единствено да регистрирува нодове в nodeeditor registry-я.

Такива плъгини се откриват от `node_editor_ide` чрез:
```cpp
QObjectList providers = pm->instances(INodeProvider_IID);
```

## Windows — специфики

Видео пайплайнът в Daqster зависи от Qt Multimedia backend-а, който се различава
между Qt5 и Qt6. По-долу са описани специфичностите за Windows.

### Qt6 (препоръчително)

- **FFmpeg backend** — работи из-кутия (bundled). RTSP стриимове се възпроизвеждат
  нативно без допълнителни инсталации. H.264 и H.265 декодерите са вградени в
  Qt6 FFmpeg build-а.
- **Хардуерно декодиране** — поддържа се през D3D11VA и DXVA2. За да включите
  HW декодиране, задайте:
  ```
  QT_FFMPEG_DECODING_HW_DEVICE_TYPES=d3d11va
  ```
  (или `dxva2` вместо `d3d11va`).
- **Zero-copy display** — `VideoFrameData` и GPU display пътят (HW буфер → RHI
  текстура → екран) са **Qt6-only** и на Windows. При активен HW decode няма
  CPU копия на кадрите.

### Qt5 (ограничена поддръжка)

- **WMF backend** — Qt5 по подразбиране използва Windows Media Foundation (WMF).
  RTSP поддръжката е ограничена или липсва в стандартния Qt5 build.
- **RTSP на Windows** — за RTSP стриимове на Qt5 се нуждае от **GStreamer-build**
  на Qt Multimedia + **GStreamer runtime** (plugins-good, plugins-bad, libav) +
  изложена променлива `GST_PLUGIN_PATH`, указваща към директорията с плъгините.
  Това изисква отделна инсталация и конфигурация.
- **Zero-copy display** — Qt5 (2026-08-13, NV12-direct) вече транспортира
  OWNED копия на декодираните кадри (`VideoCompat::frameToOwnedFrame`), така че
  `VideoFrameData` се използва и на Qt5. GPU display (detached GL blit прозорец,
  `DAQSTER_GL_BLIT=1`) работи на Qt5 с NV12/YUV420P шейдърен път; RGB формати
  падат на QImage. За максимална производителност пак се препоръчва Qt6.

### Препоръка

**Използвайте Qt6 на Windows** — осигурява нативна RTSP поддръжка, bundled
видео кодеци и zero-copy GPU display без допълнителни зависимости.

## Как да добавиш нов INodeProvider плъгин

1. Създай клас наследяващ `QBasePluginObject` + `INodeProvider`
2. Имплементирай `registerNodes()` — registriрай моделите
3. В QPluginInterface конструктора задай `PLUGIN_TYPE_NAME` (за GUI групиране)
4. В CMakeLists.txt използвай `create_plugin()` с `REQUIRES_LIBRARIES` → `QtNodes`, `frame_work`

## Свързана документация

- [IPluginInterface](../../Architecture/framework/README.md) — как плъгините се откриват
- [INodeProvider](../../Architecture/plugins/README.md) — capability discovery mechanism
- [Node Editor IDE](../node_editor_ide/README.md) — потребителят на този плъгин
  - [REQ-SW-PL-018](../../../DevelopmentProcess/requirements/active/plugins/REQ-SW-PL-018-video-source-and-processing-nodes.md) — Video Source & Processing Nodes (5-те video node модела + VideoCompat.h)
  - [REQ-SW-PL-019](../../../DevelopmentProcess/requirements/active/plugins/REQ-SW-PL-019-video-transform-node-configurable-operations.md) — Video Transform Node (премахнат 2026-08-26, операциите покрити от VideoEffectNode)
  - [REQ-SW-PL-020](../../../DevelopmentProcess/requirements/active/plugins/REQ-SW-PL-020-zero-copy-video-frame-display.md) — Zero-Copy Video Frame Transport & GPU Display (VideoFrameData, source/output nodes, Qt6 GPU display)
  - [REQ-SW-PL-024](../../../DevelopmentProcess/requirements/active/plugins/REQ-SW-PL-024-audio-source-sampleddata-migration.md) — AudioSource (Mic) миграция от QDevIO към SampledData
  - [REQ-SW-PL-028](../../../DevelopmentProcess/requirements/active/plugins/REQ-SW-PL-028-video-effect-node.md) — VideoEffectNode (GPU/CPU backend по ефект, включва blur/OpenCV)
  - [REQ-SW-PL-029](../../../DevelopmentProcess/requirements/active/plugins/REQ-SW-PL-029-custom-shader-node.md) — CustomShaderNode (runtime GLSL, GPU-only)
  - [REQ-SW-PL-030](../../../DevelopmentProcess/requirements/active/plugins/REQ-SW-PL-030-frame-sampler-node.md) — FrameSampler (ресемплиране)
  - [REQ-SW-PL-032](../../../DevelopmentProcess/requirements/active/plugins/REQ-SW-PL-032-video-frame-consolidation.md) — Video Frame Consolidation (един VideoFrameData тип, Фаза 3)
  - [REQ-SW-PL-034](../../../DevelopmentProcess/requirements/active/plugins/REQ-SW-PL-034-video-output-node-embedded-effects.md) — VideoOutputNode embedded effects (опционални, default none)

## _obsolete rename strategy_

До **[REQ-SW-PL-020]**, `QDevIO` приложението `DisplayWorld` беше преименувано на `_obsolete` (rename-стратегия) за съществена архитектурна промяна. `DaqDisplayNode` остава единственият нод с реален UI в `Displays/`, като наследник от `DisplayWorld`. Към **`1ffac96`** `QDevIO` е преименувано. При **`4ba278b`** `DaqDisplayNode` е наследник от `DisplayWorld`.

## off-GUI threading model

`DaqDisplayNode` не се стартира в GUI потребителското съземе. Комуникацията с основния интерфейс е изградена със `QThreadPool` + `Qt::QueuedConnection`, което гарантира, че FFT/канализация на графиките винаги се изпълнява в работна работа, а не в основния UI потребителски контекст. `QDataStream` изходният канал е намерен за сериализация (пре-поредица на `QVariant`). `QThread` е дефолтна възможност за асинхронизация — `DaqDisplayNode` по-специализира работата за `QDataStream` и `QPlotWidget`-ът.

### QDevIO → SampledData migration (_obsolete strategy)
Since REQ-SW-PL-023/PL-024, all legacy QDevIO components are renamed with an `_obsolete` suffix
(class `XxxObsolete`, files `XxxObsolete.{h,cpp,ui}`, registered name `XxxObsolete`, caption "(obsolete)")
but keep their original working implementation. Alias subclasses keep the old registered names where
the key is not taken by a new node. The new `DaqDisplayNode` (SampledData) and the new
`AudioSourceDataModel` (SampledData) take the current names. Both worlds coexist so capabilities and
speed can be benchmarked head-to-head; the _obsolete components are deleted at the very end.

**Display consolidation:** the SampledData display world is consolidated onto `DaqDisplayNode` as the
single canonical implementation. `GenericDisplayNode` (legacy key "GenericDisplay") and
`AudioDisplayAlias` (legacy key "AudioDisplay") are thin subclasses that override only name()/caption()
and inherit the full multi-plot/FFT/ring-buffer behavior. The "AudioDisplay" key therefore resolves to
the real SampledData display — NOT the QDevIO obsolete node. The QDevIO world stays alive under
"AudioDisplayObsolete" / "QDevIoDisplayObsolete" for old QDevIO graphs. No data work happens on the
GUI thread: capture runs in a QThread worker, decode/FFT/point-build in the shared ComputePool
(REQ-SW-PL-039), and the GUI thread only does `series->replace()` + `axis->setRange()` via a queued
result bridge.

### DAQ Display v2 (REQ-SW-PL-025)
- **Physical decode** — `SampledData::decodeToPhysical()` (header-only): `raw × amplitudeScale + amplitudeOffset`
  without normalization/centering/clamp (raw ints passthrough the scale, FLOAT32/64 passthrough value).
  `decodeToNormalizedF32` stays unchanged for the normalized per-card mode.
- **Unit axes** — per-card `QValueAxis` titles from `SampledStreamDescriptor`: Time Domain → X "Time (s)",
  Y = `descriptor.unit` (fallback "normalized"/"amplitude"); Frequency → X "Frequency (Hz)", Y = unit
  (or "Magnitude" in normalized mode). Physical Y-ranges are data-driven (min/max ± ~5% padding;
  spectrum [0, maxMag × 1.05]); normalized cards keep [-1, 1].
- **Worker-owned ring buffer** — N-second rolling per-channel raw history (default 10 s, `ringSeconds`);
  appended on each compute pass, reset on descriptor change (sampleRate/channels/bytesPerFrame);
  FFT from the ring tail (≤4096), Time Domain shows the whole window (≤2000 points). All ring access
  happens on the worker thread — no data mutex, same immutable-copy model as v1.
- **save()/restore()** — new optional fields `ringSeconds` (node) + `mode`/`unitAxes` (per card) with
  defaults (10.0 / "normalized" / true); old v1 files load unchanged (backward compatible).
