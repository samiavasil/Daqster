# REQ-SW-PL-027: Video Pipeline Instrumentation & Overlay (decode vs present timing + HW/SW markers)

- **Статус:** ACTIVE
- **Приоритет:** High
- **Отговорник (роля):** Ivan (Implementation)
- **Дата:** 2026-08-12
- **Родител:** REQ-SW-PL-018 (архив: archive/plugins/REQ-SW-PL-018-video-source-and-processing-nodes.md)
- **Зависи от:** REQ-SW-FW-008 (Lightweight Runtime Profiling Framework — нов), REQ-SW-PL-020 (VideoFrameData zero-copy type)

## Описание

REQ-SW-FW-008 въвежда runtime profiling framework-а; това изискване е **първият
реален консуматор** — видео пайплайнът (`StreamSourceNode`,
`VideoFileSourceNode`, `CameraSourceNode`, `VideoOutputNode`). Целта е да се
измери и визуализира **презентация vs доставка** (present vs gap timing) и да се
маркира **хардуерен/софтуерен** път (HW/SW) на декодирания кадър, плюс On-screen
overlay с чекбокс „Perf". Временният `VideoDiag` диагностичен блок се премахва.
Инструментирането е с нулев разход при изключен домейн (FW-008).

## Acceptance Criteria

- [ ] 1. **Source инструментиране (`StreamSourceNode`).** В
      `StreamSourceNode::onFrameAvailable` (`Sources/Video/StreamSourceNode.cpp:221`):
      `Stopwatch::mark()` за inter-frame gap (`"source.frame_interval"`) + wrap/emit
      сегмент около `setFrame()` + `dataUpdated(0)`; HW/SW маркери
      `frame.handleType()` + `frame.surfaceFormat().pixelFormat()`. Всички записи
      са в домейн `"video"` и са no-op при off.
- [ ] 2. **Output инструментиране (`VideoOutputNode`).** В
      `VideoOutputNode::setInData` (порт 0, `Sources/Video/VideoOutputNode.cpp:105`)
      около `presentFrame()` (`:127-129`): `"output.present"` (blit) + `"output.total"`
      (от входа на `setInData` до след present). Без промяна на zero-copy GPU пътя.
- [ ] 3. **Същият модел за `VideoFileSourceNode` и `CameraSourceNode`.**
      `VideoFileSourceNode::onFrameAvailable` (`VideoFileSourceNode.cpp:303`) и
      `CameraSourceNode::onFrameAvailable` (`CameraSourceNode.cpp:258`) получават
      същите `"source.frame_interval"` + wrap/emit сегменти и HW/SW маркери.
- [ ] 4. **HW/SW маркери.** `handleType`: `NoHandle` = SW,
      `RhiTextureHandle`/`GLTextureHandle` = HW; `pixelFormat` (NV12/YUV420P =
      типичен софтуер). HW decode се активира през env var
      `QT_FFMPEG_DECODING_HW_DEVICE_TYPES` (чете се от Qt FFmpeg backend при
      старт) — маркерът отразява реалното състояние на кадъра.
- [ ] 5. **Overlay + чекбокс.** Чекбокс „Perf" в `VideoOutputNode::embeddedWidget()`
      → `Domain::get("video").setEnabled(...)`; бейдж `QLabel` (child на
      detach-натия `QVideoWidget`, горе-ляво, полупрозрачен фон), обновяван на
      `QTimer` (~500 ms): `HW|SW · fmt=… · handle=… · fps=… · gap~…ms ·
      present~…ms · total~…ms`. Самото decode време НЕ е директно измеримо от
      node-а (backend-ът декодира преди `onFrameAvailable`) — атрибутира се
      външно в следващата фаза (CPU оптимизации): `top -H`/`perf` +
      `QT_LOGGING_RULES="qt.multimedia.ffmpeg.*=true"`.
- [ ] 6. **Премахване на `VideoDiag`.** Изтриване на временния диагностичен блок,
      маркиран "TEMPORARY … remove after green-screen diagnosis", в
      `StreamSourceNode.cpp:231-257` и `VideoFileSourceNode.cpp:313-339`.
- [ ] 7. **Нулев разход при изключен профил.** Инструментирането е no-op при
      runtime off (atomic toggle) и се компилира на `((void)0)`/`false` при
      `DAQSTER_ENABLE_PERF=OFF` — видео пътят не се влошава.
- [ ] 8. **Builds + smoke.** Qt5 (5.15.2) + Qt6 (6.9.2) builds PASS; app smoke
      без crash; съществуващата test suite остава зелена
      (`demo_nodeditor_nodes_tests`, `requirements_manager_tests` и др.).

## Проследимост

- **Коммити:** — (след имплементация)
- **Код:** `src/plugins/demo_nodeditor_nodes/Sources/Video/StreamSourceNode.cpp`,
  `VideoFileSourceNode.cpp`, `CameraSourceNode.cpp`, `VideoOutputNode.{h,cpp}`,
  `VideoCompat.h` (present helper), `src/frame_work/base/src/perf/` (FW-008)
- **Тестове:** отложени (standing instruction, по модела на PL-020/PL-021);
  Qt5 + Qt6 builds + app smoke; съществуващата test suite остава зелена.

## Бележки по имплементацията (план)

- **`gap` метриката:** inter-frame интервал (`source.frame_interval`) е proxy за
  декодиращата каденца (горна граница на decode), защото backend-ът декодира
  преди `onFrameAvailable` — директно decode време в node-а няма.
- **HW/SW в бейджа:** `VideoOutputNode` получава `VideoFrameData`, обвиващ
  `QVideoFrame` — `handleType()`/`pixelFormat()` се четат директно от получения
  кадър; source-side маркерите тагват собствените статистики на източника.
- **Overlay parent:** бейджът е child на detach-натия `QVideoWidget` (създаван в
  `ensureVideoWidget()`, `VideoOutputNode.cpp:277-294`) — не в node editor сцената
  (QTBUG-35299).
- **Qt5:** инструментирането е Qt6-фокусирано (zero-copy път); Qt5 остава на
  QImage пътя без overlay (без влошаване).
