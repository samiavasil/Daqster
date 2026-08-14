# REQ-SW-PL-021: In-Scene GPU Video Display via QGraphicsVideoItem (OpenGL)

- **Статус:** ACTIVE
- **Приоритет:** P1
- **Отговорник (роля):** Ivan (Implementation)
- **Дата:** 2026-08-07
- **Родител:** REQ-SW-PL-018 (архив: archive/plugins/REQ-SW-PL-018-video-source-and-processing-nodes.md)
- **Зависи от:** REQ-SW-PL-020 (VideoFrameData zero-copy type), REQ-SW-PL-014 (архив: archive/plugins/REQ-SW-PL-014-node-editor-ide-and-demo-nodes.md)

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

- [x] 1. **In-scene display.** `VideoOutputNode` на Qt6 създава `QGraphicsVideoItem`
       като дъщерен item на `NodeGraphicsObject`-а (чрез `scene->nodeGraphicsObject(nodeId)`);
       видеото се рендерира вътре в node-а, не в отделен прозорец.
       Имплементирано: `ensureSceneVideoItem()` (Qt6) — намира `GraphicsView` +
       `DataFlowGraphicsScene` през `QApplication::topLevelWidgets()`, създава
       `QGraphicsVideoItem` като child на `NodeGraphicsObject`-а (zValue над
       embedded proxy widget), геометрията следва label area-та
       (`widgetPosition` + label offset, обновява се при resize).
       Комити: `63c7f78`, `a04f05e`.
- [x] 2. **GPU path.** Кадрите се подават през `QGraphicsVideoItem::videoSink()` →
       `setVideoFrame()` (чрез `VideoCompat::presentFrame`); при активен hw decode
       (VAAPI/D3D11VA) няма QImage копие в display пътя.
       Имплементирано: `VideoOutputNode::setInData` port-0 Qt6 in-scene branch
       (`!m_detachedEnabled`) → `presentFrame(m_sceneVideoItem->videoSink(), frame)`.
       Проверено: SW decode (NoHandle) кадрите се рендерират в сцената; при HW
       decode пътят е същият zero-copy present. Комит: `a04f05e`.
- [x] 3. **Node lifetime.** `QGraphicsVideoItem` се премества/мрежи заедно с node-а
       (наследява `NodeGraphicsObject` трансформа); се трие при изтриване на node-а.
       Имплементирано: item-ът е child на `NodeGraphicsObject` (наследява
       transform); deleteLater + nullptr в деструктора, `inputConnectionDeleted`
       и при toggle обратно към detached. Комит: `a04f05e`.
- [ ] 4. **Backward compat.** Свързване с `ImageData` (processing chain) продължава
       да работи (софтуерен QLabel fallback). (Покрито частично: софтуерният
       QLabel fallback остава за image port-а и като auto-fallback; в-scene
       режимът е display-only — output-port propagation се връща в software/
       detached режимите.)
- [x] 5. **Qt5 fallback.** Qt5 ползва `QGraphicsVideoItem` + GStreamer (или detached
       `QVideoWidget` от PL-020 при недостатъчна поддръжка); поведение не се влошава.
       Анотация: Qt5 пътищата са **byte-identical** — GL blit detached по
       подразбиране / софтуерен QLabel при изключен чекбокс. QGraphicsVideoItem +
       GStreamer на Qt5 остава **future** (не се докосва в тази имплементация).
       Комити: `63c7f78`, `a04f05e` (Qt5 клоновете не са променяни семантично).
- [ ] 6. **Builds + smoke.** Qt5 + Qt6 builds PASS; app smoke без crash; съществуващата
       test suite остава зелена. Unit тестовете са **отложени** по решение на потребителя.
       Анотация: builds PASS (Qt5 5.15.2 + Qt6 6.9.2), ctest 9/9 и на двете, три
       headless smoke-а без crash (Qt6 in-scene, Qt6 detached default, Qt5 GL blit
       regression). Unit тестовете остават **отложени** (standing instruction).
- [ ] 7. **Windows документация.** README обновява Windows спецификата: Qt6 FFmpeg
       + D3D11VA; `QGraphicsVideoItem` работи на Windows Qt6. (README обновен за
       Qt6 toggle; Windows `QGraphicsVideoItem` спецификата остава за следваща
       стъпка — виж README.)

## Проследимост

- **Коммити:** `63c7f78` (Qt6 GPU display checkbox + detached/in-scene state
  separation), `a04f05e` (Qt6 in-scene QGraphicsVideoItem child of
  NodeGraphicsObject), `3a0686e` (DAQSTER_SCENE_VIDEO=1 dev hook), `ba7561f`
  (chore: remove temporary diagnostics)
- **Код:** `src/plugins/demo_nodeditor_nodes/Sources/Video/VideoOutputNode.{h,cpp}`,
  `VideoCompat.h` (present helper), `src/plugins/node_editor_ide/NodeEditorIdeObject.cpp`
  (DAQSTER_SCENE_VIDEO=1 dev driver)
- **Документация:** `docs/plugins/demo_nodeditor_nodes/README.md`
- **Тестове:** отложени (standing instruction); smoke: Qt6 in-scene / Qt6
  detached / Qt5 GL blit regression — PASS без crash, ctest 9/9 (Qt5 + Qt6).

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
