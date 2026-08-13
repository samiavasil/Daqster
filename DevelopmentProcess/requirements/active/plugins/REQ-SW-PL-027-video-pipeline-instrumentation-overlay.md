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

- [x] 1. **Source инструментиране (`StreamSourceNode`).** В
      `StreamSourceNode::onFrameAvailable` (`Sources/Video/StreamSourceNode.cpp:221`):
      `Stopwatch::mark()` за inter-frame gap (`"source.frame_interval"`) + wrap/emit
      сегмент около `setFrame()` + `dataUpdated(0)`; HW/SW маркери
      `frame.handleType()` + `frame.surfaceFormat().pixelFormat()`. Всички записи
      са в домейн `"video"` и са no-op при off.
- [x] 2. **Output инструментиране (`VideoOutputNode`).** В
      `VideoOutputNode::setInData` (порт 0, `Sources/Video/VideoOutputNode.cpp:105`)
      около `presentFrame()` (`:127-129`): `"output.present"` (blit) + `"output.total"`
      (от входа на `setInData` до след present). Без промяна на zero-copy GPU пътя.
- [x] 3. **Същият модел за `VideoFileSourceNode` и `CameraSourceNode`.**
      `VideoFileSourceNode::onFrameAvailable` (`VideoFileSourceNode.cpp:303`) и
      `CameraSourceNode::onFrameAvailable` (`CameraSourceNode.cpp:258`) получават
      същите `"source.frame_interval"` + wrap/emit сегменти и HW/SW маркери.
- [x] 4. **HW/SW маркери.** `handleType`: `NoHandle` = SW,
      `RhiTextureHandle`/`GLTextureHandle` = HW; `pixelFormat` (NV12/YUV420P =
      типичен софтуер). HW decode се активира през env var
      `QT_FFMPEG_DECODING_HW_DEVICE_TYPES` (чете се от Qt FFmpeg backend при
      старт) — маркерът отразява реалното състояние на кадъра.
- [x] 5. **Overlay + чекбокс.** Чекбокс „Perf" в `VideoOutputNode::embeddedWidget()`
      → `Domain::get("video").setEnabled(...)`; бейдж `QLabel` (child на
      detach-натия `QVideoWidget`, горе-ляво, полупрозрачен фон), обновяван на
      `QTimer` (~500 ms): `HW|SW · fmt=… · handle=… · fps=… · gap~…ms ·
      present~…ms · total~…ms`. Самото decode време НЕ е директно измеримо от
      node-а (backend-ът декодира преди `onFrameAvailable`) — атрибутира се
      външно в следващата фаза (CPU оптимизации): `top -H`/`perf` +
      `QT_LOGGING_RULES="qt.multimedia.ffmpeg.*=true"`.
- [x] 6. **Премахване на `VideoDiag`.** Изтриване на временния диагностичен блок,
      маркиран "TEMPORARY … remove after green-screen diagnosis", в
      `StreamSourceNode.cpp:231-257` и `VideoFileSourceNode.cpp:313-339`.
- [x] 7. **Нулев разход при изключен профил.** Инструментирането е no-op при
      runtime off (atomic toggle) и се компилира на `((void)0)`/`false` при
      `DAQSTER_ENABLE_PERF=OFF` — видео пътят не се влошава.
- [x] 8. **Builds + smoke.** Qt5 (5.15.2) + Qt6 (6.9.2) builds PASS; app smoke
      без crash; съществуващата test suite остава зелена
      (`demo_nodeditor_nodes_tests`, `requirements_manager_tests` и др.).

## Проследимост

- **Коммити:** `885ad11` (feat: Domain stats getters), `5c0c831` (feat:
  instrument video source nodes + remove VideoDiag), `7682bb1` (feat:
  instrument VideoOutputNode + overlay/checkbox + pure badge formatter),
  `3ecd5c0` (test: formatPerfBadge unit tests)
- **Код:** `src/plugins/demo_nodeditor_nodes/Sources/Video/StreamSourceNode.cpp`,
  `VideoFileSourceNode.cpp`, `CameraSourceNode.cpp`, `VideoOutputNode.{h,cpp}`,
  `VideoPerfBadge.{h,cpp}` (pure QtCore-only badge formatter),
  `VideoCompat.h` (present helper), `src/frame_work/base/src/perf/` (FW-008)
- **Тестове:** `formatPerfBadge` детерминистичен unit тест (8 slots, QtCore-only,
  без wall-clock — Option B): HW/SW мапинг, fps=0 edge case, ns→ms конверсия,
  enum→текст, numeric fallback, negative "no samples" → 0, exact badge layout;
  Qt5 (5.15.2) + Qt6 (6.9.2) builds PASS + ctest 9/9 green (и двете) +
  `-DDAQSTER_ENABLE_PERF=OFF` build PASS + offscreen app smoke (без crash).
  Wall-clock performance регресия (decode/present timing) НЕ се тества в ctest —
  флаки поради CPU contention и липса на GPU в CI; декодиращият backend не е
  детерминистичен (следваща фаза: CPU оптимизации + опционален harness).

## Бележки по имплементацията (план)

- **`gap` метриката:** inter-frame интервал (`source.frame_interval`) е proxy за
  декодиращата каденца (горна граница на decode), защото backend-ът декодира
  преди `onFrameAvailable` — директно decode време в node-а няма.
- **HW/SW в бейджа:** `VideoOutputNode` получава `VideoFrameData`, обвиващ
  `QVideoFrame` — `handleType()`/`pixelFormat()` се четат директно от получения
  кадър; source-side маркерите тагват собствените статистики на източника.
- **Overlay parent:** бейджът е **отделен top-level frameless tool прозорец** (не
  child на detach-натия `QVideoWidget`, създаван в `ensureVideoWidget()`), защото
  `QVideoWidget` рендерира видеото в собствен native слой (RHI swapchain) и не
  композира child widget-и върху него. Следва позицията на видео прозореца
  (горе-ляво, offset ~4,4) при създаване и на всеки ~500 ms refresh
  (`positionPerfBadge()`).
- **Qt5:** инструментирането е Qt6-фокусирано (zero-copy път); Qt5 остава на
  QImage пътя без overlay (без влошаване).
