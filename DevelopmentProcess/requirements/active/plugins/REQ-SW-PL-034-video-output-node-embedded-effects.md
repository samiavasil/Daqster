# REQ-SW-PL-034: VideoOutputNode embedded effects (optional, default none)

- **Статус:** ACTIVE
- **Приоритет:** P1
- **Отговорник (роля):** Ivan (Implementation)
- **Дата:** 2026-08-27
- **Родител:** REQ-SW-PL-018 (архив: archive/plugins/REQ-SW-PL-018-video-source-and-processing-nodes.md)
- **Зависи от:** REQ-SW-PL-028 (active/plugins/REQ-SW-PL-028-video-effect-node.md), REQ-SW-PL-032 (active/plugins/REQ-SW-PL-032-video-frame-consolidation.md)

## Описание

Display нодът (`VideoOutputNode`) получава **опционални вградени ефекти** —
същите 11 ефекта, които `VideoEffectNode` предлага (brightness, contrast,
grayscale, invert, sepia, channelSwap, flip, blur, gaussianBlur, canny,
threshold), но **без ефект по подразбиране**:

1. **Опционалност (default none):** комбобокс с водещ елемент "No effect"
   (index 0, празен id) + `m_effectEnabled` флаг. Когато ефект не е избран,
   блокът за ефекти в `setInData()` се **пропуска изцяло** — `asTexture()` /
   `asImage()` не се викат и display пътят е **byte-identical** на днешния
   (zero-copy passthrough-ът е запазен).
2. **Преизползване на съществуващите класове:** `VideoEffectOps::allSpecs()` +
   `VideoEffectGLProcessor::processTexture()` (GPU) и `EffectSpec::cpuApply`
   (CPU) се ползват директно — без извличане на shared helper.
3. **Runtime backend избор** (като `VideoEffectNode`): `GpuOrCpu` ефекти
   вървят на GPU само при хардуерен GL (`VideoGLContextManager::hasHardwareGL()`),
   иначе CPU fallback; `CpuOnly` ефекти винаги CPU.
4. **Персистентност:** ефектът + параметрите се записват в saved графа
   (`"effect"` id + `"brightness"`/`"contrast"`/`"flipMode"`/`"blurRadius"`/
   `"gaussianKernel"`/`"cannyLow"`/`"cannyHigh"`/`"thresholdValue"`).
   Стари графи без `"effect"` ключ се зареждат като "no effect" (backward
   compatible).

## Acceptance Criteria

- [x] 1. **Опционален избор на ефект с "No effect" по подразбиране.** Display
       нодът има комбобокс за ефект с водещ "No effect" елемент (index 0);
       по подразбиране ефект не е избран.
- [x] 2. **Zero-copy passthrough при липса на ефект.** Когато ефект не е
       избран, блокът за ефекти в `setInData()` се пропуска изцяло —
       `asTexture()`/`asImage()` не се викат и display пътят е byte-identical
       на версията без вградени ефекти.
- [x] 3. **Всичките 11 ефекта налични (GPU/CPU по backend).** Същите ефекти
       като `VideoEffectNode` (brightness, contrast, grayscale, invert, sepia,
       channelSwap, flip, blur + gaussianBlur/canny/threshold при `HAVE_OPENCV`);
       backend-ът се избира в рантайм (GPU при хардуерен GL, CPU иначе / за
       CpuOnly ефекти).
- [x] 4. **Ефект + параметри се персистират; backward compatible.** `save()`
       записва `"effect"` id + параметри; `load()` чете параметрите с
       default-и, клампира ги и възстановява ефекта. Стари графи без
       `"effect"` ключ → "no effect".
- [x] 5. **Qt5 + Qt6 builds PASS.**

## Проследимост

- **Коммити:** (този комит — `feat (REQ-SW-PL-034)`)
- **Код:** `src/plugins/demo_nodeditor_nodes/Sources/Video/VideoOutputNode.{h,cpp}`,
  `tests/plugins/demo_nodeditor_nodes/test_video_output_node.{h,cpp}`,
  `tests/plugins/demo_nodeditor_nodes/CMakeLists.txt`
- **Документация:** дизайн документ `video-frame-consolidation-design.md`;
  статус `2026-08-27-status.md`
- **Тестове:** `demo_nodeditor_videooutput_tests` (4 нови теста: default
  no-effect passthrough, load→save round-trip, backward compat без "effect",
  ефектът трансформира frame-а)

## Бележки по имплементацията (план)

- **Injection point:** `VideoOutputNode::setInData()` — след
  `m_videoFrame = videoFrame;` и преди `ensureVideoWidget()`.
- **GPU path:** `videoFrame->asTexture(&input)` →
  `m_glProcessor.processTexture(input, spec, m_params, &out)` →
  `VideoFrameData::fromTexture(out)` (GpuRgba, zero-copy). Само при
  `spec.backend == GpuOrCpu && VideoGLContextManager::hasHardwareGL()`.
- **CPU path (или GPU fallback):** `videoFrame->asImage()` →
  `spec.cpuApply(img, m_params)` → `std::make_shared<VideoFrameData>(QVideoFrame(transformed))`.
- **UI:** комбобокс (index 0 = "No effect", следван от ефектите с backend
  суфикс "(GPU)"/"(CPU)") + `QStackedWidget` (blank page за index 0 + по една
  параметърна страница на ефект). Компактно — widget-ът на Display нода е малък.
- **Семантика на индексите:** `setEffectIndex()` приема **combo index**
  (0 = "No effect"); `m_effectIndex` = spec index (combo index - 1, -1 = без
  ефект). `load()` мапва `indexOfEffect(id)` → combo index `+1`.