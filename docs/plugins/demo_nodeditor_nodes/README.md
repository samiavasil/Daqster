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
│   ├── AudioSource/                      # Аудио входни нодове
│   │   ├── AudioSourceDataModel.{h,cpp}
│   │   ├── AudioSourceDataModelUI.{h,cpp}
│   │   ├── AudioSourceConfig.{h,cpp,ui}
│   │   ├── AudioWorker.{h,cpp}
│   │   ├── AudioNodeQdevIoConnector.{h,cpp}
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
│   └── GenericDisplay/
│       └── GenericDisplayNode.{h,cpp}
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
        registry.registerModel<GenericDisplayNode>("Displays")
        registry.registerModel<DemuxNode>("Routing")
        registry.registerModel<MuxNode>("Routing")
        registry.registerModel<AudioSourceDataModel>("Sources")
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
| AudioSourceDataModel | Sources | Аудио вход с QWidget UI панел и QDeviceIOConnector |

### Displays
| Нод | Категория | Описание |
|-----|-----------|----------|
| AudioDisplayModel | Displays | Аудио дисплей за визуализация на аудио данни |
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
| CameraSourceNode | Video | Заснема кадри от локално camera устройство (избор на устройство + start/stop), емитира кадри — Qt6: port 0 `VideoFrameData` (zero-copy) + port 1 `ImageData` (on-demand); Qt5: `ImageData` |
| VideoFileSourceNode | Video | Възпроизвежда локален видео файл през `QMediaPlayer` + frame probe (browse + play/pause), емитира кадри — Qt6: port 0 `VideoFrameData` (zero-copy) + port 1 `ImageData` (on-demand); Qt5: `ImageData` |
| StreamSourceNode | Video | Възпроизвежда HTTP/RTSP stream (URL поле + connect), емитира кадри — Qt6: port 0 `VideoFrameData` (zero-copy) + port 1 `ImageData` (on-demand); Qt5: `ImageData` |
| VideoOutputNode | Video | Live preview на входящите кадри — Qt6: два входа (port 0 `VideoFrameData` → GPU display през `QVideoWidget`; port 1 `ImageData` → `QLabel`), zero-copy GPU път (HW буфер → RHI → екран); Qt5: един вход `ImageData` → `QLabel`. Pass-through изходен порт за output вериги |
| VideoTransformNode | Video | Прилага конфигурируема операция върху `ImageData` кадри (8 базови + опционални OpenCV операции) |

Всички Video нодове обменят данни от публичните shared NodeDataTypes (REQ-SW-PL-013). На Qt6 източниковите нодове (`CameraSourceNode`, `VideoFileSourceNode`, `StreamSourceNode`) имат два изходни порта: port 0 `VideoFrameData` ("video-frame", zero-copy) и port 1 `ImageData` ("image", конвертира се on-demand само при свързан processing потребител). `VideoOutputNode` приема и двата типа — `VideoFrameData` се дисплеира през GPU (Qt6 `QVideoWidget`), `ImageData` през софтуерен път (`QLabel`). Същият `ImageData` тип частният AI Studio plugin консумира на входа на `FrameToTensorNode` (REQ-AI-006).

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

- **Frame capture:** Qt6 `QVideoSink` (`videoFrameChanged`) vs Qt5 `QVideoProbe` (`videoFrameProbed`) — и двете доставят `QVideoFrame`, който се конвертира до `QImage`
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
- **Zero-copy display** — `VideoFrameData` не се използва на Qt5 (GStreamer
  рециклира буферите; probe кадрите не са безопасни за задържане). Qt5
  остава на QImage/`ImageData` пътя.

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
