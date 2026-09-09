# REQ-SW-PL-053: Unified Video Display (VideoDisplayWidget)

- **Статус:** ACTIVE
- **Приоритет:** High
- **Отговорник (роля):** Ivan (Implementation)
- **Дата:** 2026-09-09
- **Родител:** REQ-SW-PL-014
- **Зависи от:** REQ-SW-PL-020 (VideoFrameData zero-copy type), REQ-SW-PL-032 (frame consolidation)
- **Supersedes:** REQ-SW-PL-021 (in-scene QGraphicsVideoItem approach — архивиран)

## Описание

Унифициране на видео display-а в `VideoOutputNode` към ЕДИН интерфейс
`VideoDisplayWidget` с два backend-а:

1. **`VideoGLBlitWidget`** (GPU, default) — `QOpenGLWidget`, zero-copy
   презентация на GPU-resident текстури (RGBA/YUV), letterboxing, шейдъри.
2. **`VideoSoftwareWidget`** (CPU, fallback) — `QWidget` + `paintEvent`,
   конвертира кадъра към `QImage` (чрез `VideoFrameData::frameToImageCpu()` /
   `asImage()`) и го рендерира с keep-aspect-ratio.

Backend-ът се избира **веднъж при конструиране** (auto-detect):
`VideoGLContextManager::hasHardwareGL()` (хардуерен GL → GL blit, иначе software),
с env override `DAQSTER_VIDEO_BACKEND=gl|software`.

Display widget-ът е **дете на `m_widget`** (layout-friendly) — работи и embedded
(в node сцената / runtime workspace) и detached (плаващ прозорец чрез
съществуващия nodeeditor deembed механизъм върху `embeddedWidget()`).

**Премахва се:** in-node display (Qt5 QLabel, Qt6 `QGraphicsVideoItem`) и Qt6
native `QVideoWidget` (QTBUG-35299 — не може да се embed-не). Чекбоксът
"GPU display" се премахва (ролите му се поемат от deembed + auto-detect).

## Acceptance Criteria

- [ ] 1. **GL path.** `VideoGLBlitWidget` (GPU backend) е default при хардуерен
       GL; GpuRgba кадри → `presentTexture` (zero-copy), GpuYuv → `presentYuvTexture`,
       CPU → `presentFrame`. Видеото се показва с letterboxing.
- [ ] 2. **Software fallback.** `VideoSoftwareWidget` (CPU backend) работи без
       GPU (auto-detect при липса на хардуерен GL или `DAQSTER_VIDEO_BACKEND=software`);
       конвертира NV12/YUV420P/RGB към QImage и рендерира с keep-aspect-ratio.
- [ ] 3. **Layout-friendly / embedded.** Display widget-ът е дете на `m_widget`
       и се показва и embedded (в node-а) и detached (плаващ прозорец при deembed).
       `embeddedWidget()` продължава да връща `m_widget`.
- [ ] 4. **In-node display премахнат.** Qt5 QLabel и Qt6 `QGraphicsVideoItem`
       in-scene пътищата са премахнати от `VideoOutputNode`.
- [ ] 5. **QVideoWidget премахнат.** Qt6 native `QVideoWidget` пътят е премахнат
       (QTBUG-35299 — не може да се embed-не).
- [ ] 6. **Backend auto-detect.** `DAQSTER_VIDEO_BACKEND=gl|software` override;
       без override → `hasHardwareGL()` (cached за процеса).
- [ ] 7. **Perf badge.** Qt6 perf badge е child overlay на display widget-а
       (не top-level прозорец).
- [ ] 8. **Builds + smoke.** Qt5 + Qt6 builds PASS; app smoke без crash (GL path,
       software path, auto-fallback, detached/embedded); съществуващата test suite
       остава зелена (обновени breaking тестове; НОВИ тестове отложени по
       стоящата инструкция).

## Проследимост

- **Коммити:** (pending commit)
- **Код:** `src/plugins/demo_nodeditor_nodes/Sources/Video/VideoDisplayWidget.{h,cpp}`
  (нов), `VideoSoftwareWidget.{h,cpp}` (нов), `VideoGLBlitWidget.{h,cpp}` (refactor),
  `VideoOutputNode.{h,cpp}` (refactor), `src/plugins/common/GL/VideoGLContextManager.h`
  (`hasHardwareGL()`), `src/plugins/common/NodeDataTypes/VideoFrameData.h`
  (`frameToImageCpu()`, `asImage()`)
- **Документация:** `docs/plugins/demo_nodeditor_nodes/README.md`, `CHANGELOG.md`
- **Тестове:** обновени breaking тестове в `test_video_output_node.cpp`; нови
  тестове отложени (standing instruction)

## Бележка

Изискването е създадено по решение на потребителя (2026-09-09) след
проучването на QTBUG-35299 (Qt6 native `QVideoWidget` не може да се embed-не)
и одита на текущото състояние: `VideoOutputNode` има ТРИ display пътя (GL blit
detached, QVideoWidget detached, in-scene QGraphicsVideoItem/QLabel) с per-frame
backend switching. **Supersedes REQ-SW-PL-021** (in-scene QGraphicsVideoItem
подход — архивиран). Фаза 1 от видео display унификацията; Фаза 2 (future)
използва `model->embeddedWidget()` за workspace layout (QMdiArea/QDockWidget
per REQ-SW-PL-048/049). Unit тестове са ОТЛОЖЕНИ по стоящата инструкция
„НОВИ ТЕСТОВЕ СТОП".
