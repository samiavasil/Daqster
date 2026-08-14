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
│   └── Video/                            # Video нодове (ImageData / "image" flow)
│       ├── VideoCompat.h                 # Qt5/Qt6 multimedia compatibility shim
│       ├── CameraSourceNode.{h,cpp}
│       ├── VideoFileSourceNode.{h,cpp}
│       ├── StreamSourceNode.{h,cpp}
│       ├── VideoOutputNode.{h,cpp}
│       ├── VideoTransformNode.{h,cpp}    # 8 базови операции + опционален OpenCV
│       ├── VideoTransformOps.{h,cpp}     # Op engine (QImage + params → QImage)
│       └── OpenCVTransforms.cpp          # Само при HAVE_OPENCV
├── Displays/
│   ├── AudioDisplay/
│   │   └── AudioDisplayModel.{h,cpp}
│   └── DaqDisplay/
│       └── DaqDisplayNode.{h,cpp}
└── Routing/
    ├── Demux/
    │   └── DemuxNode.{h,cpp}
    └── Mux/
        └── MuxNode.{h,cpp}
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
        PLUGIN_VERSION    = "0.2.0"
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
  └── registerNodes(registry) →
        registry.registerModel<AudioDisplayModel>("Displays")
        registry.registerModel<DaqDisplayNode>("Displays")
        registry.registerModel<GenericDisplayNode>("Displays")
        registry.registerModel<DemuxNode>("Routing")
        registry.registerModel<MuxNode>("Routing")
        registry.registerModel<AudioSourceDataModel>("Sources")
        registry.registerModel<AudioSourceDataModelObsolete>("Sources")
        registry.registerModel<LLamaModelDataModel>("LLama")
        registry.registerModel<ConsoleDataModel>("LLama")
        registry.registerModel<CameraSourceNode>("Video")
        registry.registerModel<VideoFileSourceNode>("Video")
        registry.registerModel<StreamSourceNode>("Video")
        registry.registerModel<VideoOutputNode>("Video")
        registry.registerModel<VideoTransformNode>("Video")
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
| AudioDisplayModel | Displays | Аудио дисплей за визуализация на аудио данни |
| DaqDisplayNode | Displays | Реален Qt Charts waveform + FFT за всякarn плъгин с sampled данни (audio/DAQ/sензори). v2 (REQ-SW-PL-025): физически decode (`decodeToPhysical`, `raw × amplitudeScale + amplitudeOffset`), unit оси от дескриптора (Time (s)/Hz + мерна единица), worker-притежаван N-секунден ring buffer (default 10 s) с FFT от опашката; per-card `mode` (normalized/physical) + `unitAxes`, backward-compatible save/restore |
| GenericDisplayNode | Displays | Универсален дисплей за generic данни |

### Routing
| Нод | Категория | Описание |
|-----|-----------|----------|
| DemuxNode | Routing | Demultiplexer — разделя един input stream на множество output-и |
| MuxNode | Routing | Multiplexer — комбинира множество input streams в един output |

### LLama
| Нод | Категория | Описание |
|-----|-----------|----------|
| LLamaModelDataModel | LLama | LLaMA модел — AI генерация на текст |
| ConsoleDataModel | LLama | Конзола за показване на AI отговори |

### Video
| Нод | Категория | Описание |
|-----|-----------|----------|
| CameraSourceNode | Video | Заснема кадри от локално camera устройство (избор на устройство + start/stop), емитира кадри — port 0 `VideoFrameData` (Qt6: zero-copy; Qt5: OWNED copy) + port 1 `ImageData` (on-demand) |
| VideoFileSourceNode | Video | Възпроизвежда локален видео файл през `QMediaPlayer` + frame probe (browse + play/pause), емитира кадри — port 0 `VideoFrameData` (Qt6: zero-copy; Qt5: OWNED copy) + port 1 `ImageData` (on-demand) + port 2 `SampledData` (audio) |
| StreamSourceNode | Video | Възпроизвежда HTTP/RTSP stream (URL поле + connect), емитира кадри — port 0 `VideoFrameData` (Qt6: zero-copy; Qt5: OWNED copy) + port 1 `ImageData` (on-demand) + port 2 `SampledData` (audio) |
| VideoOutputNode | Video | Live preview на входящите кадри — два входа (port 0 `VideoFrameData` → GPU display; port 1 `ImageData` → `QLabel`), zero-copy GPU път. Qt6: чекбокс „GPU display" (checked по подразбиране) → detached прозорец (`QVideoWidget` или GL blit при `DAQSTER_GL_BLIT=1`); unchecked → in-scene `QGraphicsVideoItem` в node-а (REQ-SW-PL-021). Qt5: checked → detached GL blit прозорец, unchecked → софтуерен QLabel. Pass-through изходен порт за output вериги |
| VideoTransformNode | Video | Прилага конфигурируема операция върху `ImageData` кадри (8 базови + опционални OpenCV операции) |

Всички Video нодове обменят данни от публичните shared NodeDataTypes (REQ-SW-PL-013). Източниковите нодове (`CameraSourceNode`, `VideoFileSourceNode`, `StreamSourceNode`) имат port 0 `VideoFrameData` ("video-frame", zero-copy) и port 1 `ImageData` ("image", конвертира се on-demand само при свързан processing потребител); видео source-ите имат и port 2 `SampledData` (audio, appended last — REQ-SW-PL-022 AC 8). `VideoOutputNode` приема и двата типа — `VideoFrameData` се дисплейва през GPU (вж. „Qt6 in-scene toggle" по-долу; Qt5: detached GL blit прозорец при `DAQSTER_GL_BLIT=1`), `ImageData` през софтуерен път (`QLabel`). Същият `ImageData` тип частният AI Studio plugin консумира на входа на `FrameToTensorNode` (REQ-AI-006).

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
> "video-frame", а image портът е port 1 (беше port 0 на Qt5). Стари saved Qt5
> графи, които свързват source port 0 (беше "image") с ImageData консуматор,
> **ще загубят връзката** и трябва да се пресвържат към новия image порт (1).
> Причина: Qt5 сега транспортира OWNED NV12/YUV420P копия (`VideoCompat::frameToOwnedFrame`),
> така че `VideoFrameData` не е вече Qt6-only.

#### VideoTransformNode — операции (REQ-SW-PL-019)

`VideoTransformNode` заменя стария фиксиран `VideoModifierNode` (само R↔B swap) с общ transform node. UI-то е operation combo + `QStackedWidget` с по една страница параметри за операция; текущата операция и параметрите се персистират чрез `save()`/`load()`.

**8 базови операции (QImage, винаги налични, без външни зависимости):**

| Операция | Параметри | Описание |
|----------|-----------|----------|
| RGB Channel Swap | — | Размяна на R↔B каналите на всеки пиксел |
| Grayscale | — | Конвертиране към grayscale (luminance), изход RGB32 |
| Invert | — | Инвертиране на цветовете на всеки пиксел |
| Sepia | — | Sepia тон |
| Brightness | slider −100..+100 (0 = без промяна) | Регулиране на яркостта |
| Contrast | slider 0..200% (100 = без промяна) | Регулиране на контраста |
| Blur | radius slider 0..10 (0 = без промяна) | Box blur |
| Flip | combo horizontal/vertical | Обръщане хоризонтално или вертикално |

**3 опционални OpenCV операции (само при `HAVE_OPENCV`):**

| Операция | Параметри | Описание |
|----------|-----------|----------|
| GaussianBlur | kernel slider 1..31 (нечетен) | Gaussian blur |
| Canny | low/high threshold sliders 0..255 | Canny edge detection |
| Threshold | value slider 0..255 | Binary threshold |

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
- **OpenCV 4.x (ОПЦИОНАЛЕН)** — само за OpenCV video transform операциите (GaussianBlur, Canny, Threshold)

### OpenCV (опционален)

`find_package(OpenCV QUIET)` в `CMakeLists.txt` + `option(DAQSTER_USE_OPENCV ... ON)`. Когато `DAQSTER_USE_OPENCV=ON` и OpenCV е открит:
- `OpenCVTransforms.cpp` се компилира и дефинира `HAVE_OPENCV`
- OpenCV операциите (GaussianBlur, Canny, Threshold) се добавят към списъка на `VideoTransformNode`
- `${OpenCV_INCLUDE_DIRS}` / `${OpenCV_LIBS}` се подават към `create_plugin()`

Когато OpenCV липсва или `DAQSTER_USE_OPENCV=OFF`, plugin-ът се build-ва нормално с 8-те базови QImage операции; OpenCV операциите отсъстват от списъка. Проверено: Qt5 (5.15.2) + Qt6 (6.9.2) builds PASS и с двата варианта (OpenCV 4.6.0 / без OpenCV).

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
  - [REQ-SW-PL-019](../../../DevelopmentProcess/requirements/active/plugins/REQ-SW-PL-019-video-transform-node-configurable-operations.md) — Video Transform Node (8 базови + опционални OpenCV операции)
  - [REQ-SW-PL-020](../../../DevelopmentProcess/requirements/active/plugins/REQ-SW-PL-020-zero-copy-video-frame-display.md) — Zero-Copy Video Frame Transport & GPU Display (VideoFrameData, dual-port source/output nodes, Qt6 GPU display)
  - [REQ-SW-PL-024](../../../DevelopmentProcess/requirements/active/plugins/REQ-SW-PL-024-audio-source-sampleddata-migration.md) — AudioSource (Mic) миграция от QDevIO към SampledData

## _obsolete rename strategy_

До **[REQ-SW-PL-020]**, `QDevIO` приложението `DisplayWorld` беше преименувано на `_obsolete` (rename-стратегия) за съществена архитектурна промяна. `DaqDisplayNode` остава единственият нод с реален UI в `Displays/`, като наследник от `DisplayWorld`. Към **`1ffac96`** `QDevIO` е преименувано. При **`4ba278b`** `DaqDisplayNode` е наследник от `DisplayWorld`.

## off-GUI threading model

`DaqDisplayNode` не се стартира в GUI потребителското съземе. Комуникацията с основния интерфейс е изградена със `QThreadPool` + `Qt::QueuedConnection`, което гарантира, че FFT/канализация на графиките винаги се изпълнява в работна работа, а не в основния UI потребителски контекст. `QDataStream` изходният канал е намерен за сериализация (пре-поредица на `QVariant`). `QThread` е дефолтна възможност за асинхронизация — `DaqDisplayNode` по-специализира работата за `QDataStream` и `QPlotWidget`-ът.

### QDevIO → SampledData migration (_obsolete strategy)
Since REQ-SW-PL-023/PL-024, all legacy QDevIO components are renamed with an `_obsolete` suffix
(class `XxxObsolete`, files `XxxObsolete.{h,cpp,ui}`, registered name `XxxObsolete`, caption "(obsolete)")
but keep their original working implementation. Alias subclasses keep the old registered names where
the key is not taken by a new node (e.g. `AudioDisplayModelObsoleteAlias` → "AudioDisplay").
The new `DaqDisplayNode` (SampledData) and the new `AudioSourceDataModel` (SampledData) take the
current names. Both worlds coexist so capabilities and speed can be benchmarked head-to-head;
the _obsolete components are deleted at the very end. No data work happens on the GUI thread:
capture runs in a QThread worker, decode/FFT/point-build in a node-owned QThreadPool (maxThreadCount=1),
and the GUI thread only does `series->replace()` + `axis->setRange()` via a queued result bridge.

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
