# REQ-SW-PL-018: Video Source & Processing Nodes for the Demo Node Editor

- **Статус:** ACTIVE
- **Приоритет:** P2
- **Отговорник (роля):** Ivan (Implementation)
- **Дата:** 2026-08-06
- **Родител:** REQ-SW-PL-014
- **Зависи от:** REQ-SW-PL-013

## Описание

Demo node editor-ът (`demo_nodeditor_nodes`) няма video възможности: липсват
node типове за заснемане/възпроизвеждане на видео и за обработка на кадри.
Това изискване добавя **5 video node модела** в категория "Video" + един
Qt5/Qt6 multimedia compatibility shim (`VideoCompat.h`), всички обменящи
`ImageData` ("image") от публичните shared NodeDataTypes (REQ-SW-PL-013):

1. **CameraSourceNode** — заснема кадри от локално camera устройство
   (избор на устройство + start/stop), емитира `ImageData` с честотата на
   кадрите.
2. **VideoFileSourceNode** — възпроизвежда локален видео файл през
   `QMediaPlayer` + frame probe (browse + play/pause), емитира кадри като
   `ImageData`.
3. **StreamSourceNode** — възпроизвежда HTTP/RTSP stream (URL поле +
   connect), емитира кадри като `ImageData`.
4. **VideoOutputNode** — live preview на входящите `ImageData` кадри в
   `QLabel`; pass-through изходен порт за изграждане на output вериги.
5. **VideoModifierNode** — демо трансформация: размяна на R↔B каналите на
   всеки кадър, емитира модифицираното `ImageData`.

`VideoCompat.h` абстрахира Qt5/Qt6 multimedia API различията по модела на
съществуващия `AudioCompat.h`: frame capture (Qt5 `QVideoProbe` /
`videoFrameProbed` vs Qt6 `QVideoSink` / `videoFrameChanged`), camera
enumeration (Qt5 `QCameraInfo` vs Qt6 `QMediaDevices`), media source
assignment (Qt5 `setMedia(QMediaContent)` vs Qt6 `setSource(QUrl)`),
playback-state и error сигнали, нормализирани като int/callback — така
caller-ите остават version-agnostic.

## Acceptance Criteria

- [x] 1. **`VideoCompat.h` Qt5/Qt6 multimedia shim.** Namespace `VideoCompat`
       (`Sources/Video/VideoCompat.h`) с type aliases и inline helpers:
       `FrameProbe` (`QVideoSink`/`QVideoProbe`), `availableCameras()`
       / `defaultCamera()` / `cameraId()` / `cameraDescription()`,
       `attachFrameProbe(QCamera*|QMediaPlayer*, FrameProbe*)`,
       `frameToImage()`, `setMediaSource()`, `connectFrameProbed()`,
       `connectPlaybackState()`, `connectPlayerError()`, `connectCameraError()`.
       Компилира се и на Qt5 (5.15.2), и на Qt6 (6.9.2).
- [x] 2. **CameraSourceNode.** `Sources/Video/CameraSourceNode.{h,cpp}` —
       `NodeDelegateModel`, `name() = "CameraSource"`, embedded widget с
       device combo + start/stop, `onFrameAvailable()` конвертира
       `QVideoFrame` → `QImage` → `ImageData`; `save()`/`load()` сериализират
       избраното устройство.
- [x] 3. **VideoFileSourceNode.** `Sources/Video/VideoFileSourceNode.{h,cpp}` —
       `name() = "VideoFileSource"`, file dialog (`.mp4 .avi .mkv .mov
       .webm .m4v .mpg .mpeg`), `VideoCompat::setMediaSource()`,
       статуси End of video / Invalid file, емитира `ImageData`.
- [x] 4. **StreamSourceNode.** `Sources/Video/StreamSourceNode.{h,cpp}` —
       `name() = "StreamSource"`, URL поле + connect бутон, `QMediaPlayer` +
       frame probe, емитира `ImageData`.
- [x] 5. **VideoOutputNode.** `Sources/Video/VideoOutputNode.{h,cpp}` —
       `name() = "VideoOutput"`, live preview в `QLabel` + pass-through
       изходен порт (output вериги, напр. изход на modifier).
- [x] 6. **VideoModifierNode.** `Sources/Video/VideoModifierNode.{h,cpp}` —
       `name() = "VideoModifier"`, демо ефект `swapRedBlueChannels()` (R↔B)
       върху всеки входящ кадър, емитира модифицирано `ImageData`.
       **Амендирано от REQ-SW-PL-019:** node-ът е преименуван на
       `VideoTransformNode` (регистрация `"VideoTransform"`); R↔B swap-ът е
       една от 8-те базови операции на новия node (виж REQ-SW-PL-019).
- [x] 7. **Регистрация + build.** `DemoNodeEditorNodesObject::registerNodes()`
       регистрира всичките 5 под категория "Video"; `CMakeLists.txt` включва
       новите файлове; `Sources/Video` е в `INCLUDE_DIRECTORIES`.
- [ ] 8. **Tests.** Unit тестове за video node моделите (ако/когато бъдат
       разрешени): frame conversion (`VideoCompat::frameToImage`),
       modifier R↔B swap върху известен пиксел, port/type контракти.
       Qt5 + Qt6 builds; съществуващата suite остава зелена.

## Проследимост

- **Коммити:** `e5b7309`, `ef1b66c` (branch `feat/phase3-graph-matrix`)
- **Код:** `src/plugins/demo_nodeditor_nodes/Sources/Video/` (`VideoCompat.h`,
  `CameraSourceNode.{h,cpp}`, `VideoFileSourceNode.{h,cpp}`,
  `StreamSourceNode.{h,cpp}`, `VideoOutputNode.{h,cpp}`,
  `VideoModifierNode.{h,cpp}`), `DemoNodeEditorNodesObject.cpp`
  (registerNodes), `CMakeLists.txt`
- **Документация:** `docs/plugins/demo_nodeditor_nodes/README.md` (Video секция)
- **Тестове:** Qt5 (5.15.2) + Qt6 (6.9.2) builds PASS; app smoke —
  приложенията стартират без crash; unit тестовете са отложени

## Бележки по имплементацията (план)

- `VideoCompat.h` следва точно модела на `AudioCompat.h` (вече съществуващ в
  plugin-а): namespace от type aliases + inline helpers, един
  version-agnostic API за caller-ите.
- Qt 5.15 `QMediaPlayer::error(Error)` няма error string —
  `QOverload<QMediaPlayer::Error>::of(&QMediaPlayer::error)` развързва
  сигнала от accessor-а.
- Qt6 camera routing изисква `QMediaCaptureSession`, създаден като child на
  sink-а, за да съвпада lifetime-ът с frame capture-а.
- И двата playback-state enum-а (Qt5 `State`, Qt6 `PlaybackState`) споделят
  едни и същи unscoped стойности (напр. `QMediaPlayer::PlayingState`) —
  транспортират се като int.
- Video node-ите обменят `ImageData` от публичните NodeDataTypes
  (REQ-SW-PL-013) — същия тип, който private AI Studio plugin-ът консумира
  на входа на `FrameToTensorNode` (REQ-AI-006).
- `Документация:` сочи `docs/plugins/demo_nodeditor_nodes/README.md` (Video секция)
  → главният изглед ще покаже „Архитектура ✓". (PL-017: непразно поле = записано,
  НЕ = верифицирано.)

## Бележка

Имплементацията е завършена (2026-08-06) и верифицирана: Qt5 (5.15.2) + Qt6
(6.9.2) builds PASS, приложенията стартират без crash. Unit тестовете са
отложени по решение на автора (standing instruction: имплементации без
тестове до ново нареждане). Статусът остава ACTIVE — DONE изисква пълна
верификация (Qt5 + Qt6 builds + unit тестове + headless smoke test) по
RDD-PROCESS.md.
