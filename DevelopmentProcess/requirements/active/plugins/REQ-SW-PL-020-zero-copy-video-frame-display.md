# REQ-SW-PL-020: Zero-Copy Video Frame Transport & GPU Display (VideoFrameData)

- **Статус:** ACTIVE
- **Приоритет:** P1
- **Отговорник (роля):** Ivan (Implementation)
- **Дата:** 2026-08-07
- **Родител:** REQ-SW-PL-018
- **Зависи от:** REQ-SW-PL-018, REQ-SW-PL-013

## Описание

Текущият video пайплайн конвертира **всеки** декодиран кадър до `QImage` още в
source node-а (`QVideoFrame::toImage()`) и display node-ът допълнително прави
`QPixmap::fromImage()` + `scaled(SmoothTransformation)` на всеки кадър —
4–6 излишни пълнокадрови CPU операции на кадър, всички на GUI нишката, без
throttling. При 1080p30 това насища едно ядро (наблюдавано >100% CPU на 8-ядрена
машина).

Това изискване **разделя display path-а от processing path-а**:

1. **Zero-copy транспорт:** нов `VideoFrameData` NodeData тип (обвивка на Qt6
   `QVideoFrame`) се транспортира през графа като `shared_ptr` — копира се
   указателят, не буферът (дизайнът вече е приет в
   `AGENT-KNOWLEDGE.md`). Qt6 `QVideoFrame` е ref-counted (`QSharedData`) —
   безопасно за държане и предаване.
2. **Display path (лекият):** `VideoOutputNode` на Qt6 презентира кадрите
   директно през GPU — декодираният HW буфер → RHI текстура → екран, **без
   QImage, без CPU конверсия**. Node-editor-ът не може да хостингне
   `QVideoWidget` в сцената (QTBUG-35299; Qt6 `QVideoWidget` е native `QWindow`
   със собствен RHI swapchain) → ползва се вграденият detach механизъм на
   nodeeditor-а: при първи кадър `QVideoWidget` се детачва в отделен top-level
   прозорец; в node-а остава софтуерен preview като fallback.
3. **Processing path (тежкият):** обработващите нодове (`VideoTransform`,
   `FrameToTensor`) остават на `ImageData` — QImage конверсия само когато са
   свързани в графа.
4. **Qt5:** поведението остава непроменено (QImage път) — Qt5 probe frames са
   рискови за задържане (GStreamer рециклира буферите), така че
   `VideoFrameData` е **Qt6-gated**.
5. **Windows:** Qt6 FFmpeg backend работи из-кутия (RTSP нативно, кодек -ите
   bundled); спецификите се документират в README-а на plugin-а.

## Acceptance Criteria

- [x] 1. **`VideoFrameData` тип.** NodeData subclass в
       `src/plugins/common/NodeDataTypes/`, `type()` → `{"video-frame",
       "Video Frame"}`, обвива `QVideoFrame` (Qt6). В Qt5 среда типът не се
       използва (guard).
- [x] 2. **Source нодовете (Qt6) емитират `VideoFrameData`.** `StreamSourceNode`,
       `CameraSourceNode`, `VideoFileSourceNode` на Qt6 не викат `toImage()` в
       hot path-а — емитират `VideoFrameData`. Когато downstream иска
       `ImageData` (processing консуматор), конверсията става в консуматора,
       не в източника.
- [x] 3. **`VideoOutputNode` GPU display (Qt6).** При първи кадър отваря
       `QVideoWidget`, детачнат в отделен прозорец (вграден detach механизъм),
       захранван през `videoSink()`; кадрите стигат до екрана през
       HW-буфер → RHI → екран (GPU path, нула CPU копия при активен hw
       decode). In-node placeholder показва последния софтуерен кадър като
       fallback.
- [x] 4. **Processing веригата непроменена.** `VideoTransformNode` (REQ-SW-PL-019)
       и `FrameToTensorNode` (REQ-AI-006) продължават да работят с `ImageData` —
       QImage конверсия само когато са свързани.
- [x] 5. **Qt5 поведение непроменено.** Qt5 остава на QImage/`ImageData` пътя
       (GStreamer probe, безопасна незабавна конверсия).
- [ ] 6. **Qt5 + Qt6 builds PASS + app smoke.** Приложенията стартират без
       crash; и двете версии се build-ват. Unit тестовете са **отложени по
       решение на потребителя** (standing instruction — status → `DONE` чака
       тестовете).
- [x] 7. **Windows документация.** `docs/plugins/demo_nodeditor_nodes/README.md`
       документира Windows особеностите на video пайплайна: Qt6 FFmpeg backend
       работи из-кутия (RTSP нативно, без допълнителни инсталации); Qt5 ползва
       WMF backend с ограничена/липсваща RTSP поддръжка — за RTSP на Windows
       трябва GStreamer-build на Qt Multimedia + GStreamer runtime
       (plugins-good/bad/libav) + `GST_PLUGIN_PATH`; хардуерното декодиране на
       Windows е през D3D11VA/DXVA2; `VideoFrameData` е Qt6-only и на Windows.

## Проследимост

- **Коммити:** `085f63d` (feat: VideoOutputNode dual-input + Qt6 GPU display),
  `0f92a9c` (build: link MultimediaWidgets), `c873b43` (docs: Windows specifics
  + traceability matrix) — branch `feat/REQ-SW-PL-020-video-frame-display`
- **Код:** `src/plugins/common/NodeDataTypes/VideoFrameData.h`,
  `src/plugins/demo_nodeditor_nodes/Sources/Video/` (source нодове,
  `VideoOutputNode`, `VideoCompat.h` — shim функции за present),
  `CMakeLists.txt`
- **Документация:** `docs/plugins/demo_nodeditor_nodes/README.md` (Windows
  specifics + dual-port data flow)
- **Тестове:** отложени по решение на потребителя (standing instruction). Qt5
  (5.15.2) + Qt6 (6.9.2) builds PASS; app smoke без crash; съществуващата test
  suite остава зелена (demo_nodeditor_nodes_tests 29/29,
  requirements_manager_tests 87/87, exporter 7/7, matrix 8/8, gui 4/4 — на
  двете версии).
- **Bug fix (post-impl):** `VideoOutputNode::setInData()` port 0 (Qt6) изпълняваше
  per-frame QImage конверсия + QLabel update + ImageData output дори когато GPU
  пътят (detached QVideoWidget) е активен — двойна CPU работа. Поправка:
  `outputConnectionCreated`/`outputConnectionDeleted` следят брой connections
  на output port 0 (`m_outputConnectionCount`); QImage конверсията + ImageData
  output се изпълняват само когато `m_outputConnectionCount > 0`; QLabel-ът
  показва статичен placeholder ("GPU display active — see detached window")
  веднъж при първи кадър.

## Бележки по имплементацията (план)

- **Transport е вече zero-copy.** QtNodes подава `std::shared_ptr<NodeData>`
  (refcount bump, без копие на буфера). Веригата на GUI нишката е
  synchronous/direct — няма queued-connection риск между нодовете; рискът е
  само backend → source (вече текущата архитектура).
- **`VideoCompat.h`** е designated shim за Qt5/Qt6 multimedia — всички
  version-разлики (вкл. present/QVideoSink helper) отиват там.
- **Qt5 gotcha:** `QVideoProbe` frames не трябва да се държат извън сигнала;
  `flush()` съществува точно за освобождаване на задържани референции. Затова
  нулево-копийният път е Qt6-only.
- **QGraphicsProxyWidget не може да хостингне `QVideoWidget`** (QTBUG-35299 на
  Qt5 — "can't work, and never will"; Qt6 `QVideoWindow` = native QWindow със
  собствен RHI swapchain). Решението е detached top-level прозорец (съществува
  `NodeGraphicsObject` detach path) или `QGraphicsVideoItem` (изисква nodeeditor
  разширение — извън обхват за това изискване).
- **HW decode:** Qt6.9.2 FFmpeg backend поддържа VAAPI (Linux) / D3D11VA (Win);
  включва се с `QT_FFMPEG_DECODING_HW_DEVICE_TYPES=vaapi`. Без hw decode
  GPU display пак спестява QImage/QPixmap копията.
- **Frame rate throttling** е допълнителна оптимизация (извън обхват тук, но
  препоръчана като follow-up заедно с `VideoTransform` CPU профила).

## Бележка

Изискването е създадено **преди** имплементацията (2026-08-07) по одобреното
от потребителя решение: разделяне на display (zero-copy, GPU) и processing
(QImage) пътища. Потребителят е наредил unit тестовете да се отложат за това
изискване (standing instruction) — статус → `DONE` чака тестовете. Процесната
клауза "branch per work item" (AGENTS.md) важи: работата се върши на нов
branch `feat/REQ-SW-PL-020-video-frame-display`.

**Статус:** ACTIVE (имплементация завършена 2026-08-07; unit тестовете
отложени по решение на потребителя). AC 1–5, 7 `[x]`; AC 6 `[ ]` (tests
deferred).
