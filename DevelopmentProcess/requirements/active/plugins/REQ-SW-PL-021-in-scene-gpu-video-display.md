# REQ-SW-PL-021: In-Scene GPU Video Display via QGraphicsVideoItem (OpenGL)

- **Статус:** ACTIVE
- **Приоритет:** P1
- **Отговорник (роля):** Ivan (Implementation)
- **Дата:** 2026-08-07
- **Родител:** REQ-SW-PL-018
- **Зависи от:** REQ-SW-PL-020 (VideoFrameData zero-copy type), REQ-SW-PL-014 (Node Editor IDE)

## Описание

Вмъкване на видеото директно в NodeEditor-а (in-scene) чрез `QGraphicsVideoItem`
като дъщерен `QGraphicsItem` на `NodeGraphicsObject`-а, вместо detached
`QVideoWidget`. Това постига:
1. **Консистентен UX** — видеото е вътре в node-а, не в отделен прозорец.
2. **GPU display** — `QGraphicsVideoItem` рендерира HW декодирания кадър директно
   през RHI/OpenGL (VAAPI Linux / D3D11VA Windows), без QImage копие.
3. **Без рефактор на nodeeditor библиотеката** — `BasicGraphicsScene::nodeGraphicsObject(NodeId)`
   (вече съществува) връща `NodeGraphicsObject*`, който наследява `QGraphicsObject`
   и може да има дъщери `QGraphicsItem`-и.

Архитектура:
- `VideoOutputNode` (Qt6) получава сцената + NodeId (чрез `DataFlowGraphicsScene`
  сигнал/метод); при първи `VideoFrameData` създава `QGraphicsVideoItem` като
  дъщерен item на своя `NodeGraphicsObject`; храни кадрите през
  `videoItem->videoSink()->setVideoFrame()` (или `VideoCompat::presentFrame`).
- `QGraphicsVideoItem` се движи/мрежи заедно с node-а (наследява трансформа).
- Qt5: `QGraphicsVideoItem` + GStreamer (VAAPI); Qt5 `QVideoFrame` lifetime риск
  — използва се `QGraphicsVideoItem` който сам управлява кадрите (не държим
  QVideoFrame в NodeData).
- При липса на `QGraphicsVideoItem` (fallback) — detached `QVideoWidget` от
  REQ-SW-PL-020.

## Acceptance Criteria

- [ ] 1. **In-scene display.** `VideoOutputNode` на Qt6 създава `QGraphicsVideoItem`
       като дъщерен item на `NodeGraphicsObject`-а (чрез `scene->nodeGraphicsObject(nodeId)`);
       видеото се рендерира вътре в node-а, не в отделен прозорец.
- [ ] 2. **GPU path.** Кадрите се подават през `QGraphicsVideoItem::videoSink()` →
       `setVideoFrame()`; при активен hw decode (VAAPI/D3D11VA) няма QImage копие
       в display пътя.
- [ ] 3. **Node lifetime.** `QGraphicsVideoItem` се премества/мрежи заедно с node-а
       (наследява `NodeGraphicsObject` трансформа); се трие при изтриване на node-а.
- [ ] 4. **Backward compat.** Свързване с `ImageData` (processing chain) продължава
       да работи (софтуерен QLabel fallback).
- [ ] 5. **Qt5 fallback.** Qt5 ползва `QGraphicsVideoItem` + GStreamer (или detached
       `QVideoWidget` от PL-020 при недостатъчна поддръжка); поведение не се влошава.
- [ ] 6. **Builds + smoke.** Qt5 + Qt6 builds PASS; app smoke без crash; съществуващата
       test suite остава зелена. Unit тестовете са **отложени** по решение на потребителя.
- [ ] 7. **Windows документация.** README обновява Windows спецификата: Qt6 FFmpeg
       + D3D11VA; `QGraphicsVideoItem` работи на Windows Qt6.

## Проследимост

- **Коммити:** — (след имплементация)
- **Код:** `src/plugins/demo_nodeditor_nodes/Sources/Video/VideoOutputNode.{h,cpp}`,
  `VideoCompat.h` (present helper), `DemoNodeEditorNodesObject.cpp` (scene/NodeId достъп)
- **Документация:** `docs/plugins/demo_nodeditor_nodes/README.md`
- **Тестове:** отложени (standing instruction).

## Бележни по имплементацията (план)

- **Scene/NodeId достъп:** `NodeDelegateModel` няма прям достъп до сцената. Трябва
  да се получи през `DataFlowGraphicsScene` (провери как `DemoNodeEditorNodesObject`
  регистрира/създава нодовете) — обикновено чрез сигнал `nodeCreated(NodeId)` или
  `graphModel().nodeData(nodeId, NodeRole::...)`. Алтернатива: `QApplication::topLevelWidgets()`
  → намери `GraphicsView` → `viewport()` → `scene()` → `nodeGraphicsObject(nodeId)`.
- **QGraphicsVideoItem lifetime:** създава се веднъж (lazy) и се позиционира/мрежи
  при `itemChange`/geometry update; приема `setParentItem(nodeGraphicsObject)`.
- **Qt5 QVideoFrame lifetime:** `QGraphicsVideoItem` управлява кадрите във вътрешния си
  sink — не държим QVideoFrame в NodeData, така че Qt5 probe lifetime рискът е избегнат.
- **OpenGL viewport:** `QGraphicsVideoItem` работи с raster viewport, но за пълен GPU
  път (HW текстура → текстура) Qt6 RHI backend-ът в `libffmpegmediaplugin.so` управлява
  това автоматично. Няма нужда от смяна на viewport-а за базовата версия.

## Бележка

Изискването е създадено **преди** имплементацията (2026-08-07) по одобрения дизайн
(in-scene QGraphicsVideoItem, без nodeeditor рефактор). Потребителят одобри
опция C (OpenGL viewport / in-scene embedding) и потвърди желанието за embed
в NodeEditor-а. Unit тестовете са отложени (standing instruction).
