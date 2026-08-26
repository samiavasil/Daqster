# REQ-SW-PL-032: VideoFrameData консолидация — един тип с lazy кешове

- **Статус:** ACTIVE
- **Приоритет:** P1
- **Отговорник (роля):** Ivan (Implementation)
- **Дата:** 2026-08-24
- **Родител:** REQ-SW-PL-020 (active/plugins/REQ-SW-PL-020-zero-copy-video-frame-display.md)
- **Зависи от:** REQ-SW-PL-028 (active/plugins/REQ-SW-PL-028-video-effect-node.md), REQ-SW-PL-030 (active/plugins/REQ-SW-PL-030-frame-sampler-node.md)

## Описание

Консолидация на video frame типовете (`ImageData` vs `VideoFrameData`) в
**един тип `VideoFrameData`** с **lazy кешове** (QImage + GL текстура):

1. **Payload:** `QVideoFrame` (native buffer).
2. **Lazy `QImage` кеш:** `asImage()` — конвертира веднъж, кешира, споделя.
3. **Lazy GL текстура кеш:** за GPU display/processing.
4. **Display** = `frame()` — zero-copy (native buffer).
5. **Processing** = `asImage()` — 1×/кадър, споделено между всички
   консуматори.
6. **„Най-много едно копие на представяне на кадър, lazy и споделено.“**
7. **Zero-copy само в рамките на един домейн** (GPU домейн или CPU домейн) —
   не между домейните (GPU→CPU копие е неизбежно при processing).

**Staged миграция (3 фази):**
- **Фаза 1 — паралелно:** `ImageData` остава; новите нодове
  (`VideoEffectNode`, `CustomShaderNode`, `FrameSampler`, `LoadPicture`)
  работят с `VideoFrameData`, старите с `ImageData` — без да чупят
  съществуващите графи.
- **Фаза 2 — еквивалентност:** доказана чрез **ръчна оценка от потребителя**
  (визуално; без пиксел-диф харнес) — решение 2026-08-24.
- **Фаза 3 — миграция:** `VideoTransformNode`, `FrameToTensorNode`,
  `VideoOutputNode` fallback, saved графи мигрирани към един тип.

## Уточнен дизайн (2026-08-25)

Уточнения на дизайна от дискусията (записват се като част от изискването):

**а) Lazy кешове в `VideoFrameData`:**
- `asImage()` — lazy CPU QImage кеш: конверсията става веднъж на кадър,
  резултатът се кешира и се споделя между всички CPU консуматори.
- `asTexture()` — lazy GPU текстура кеш: upload-ът става веднъж на кадър,
  текстурата се кешира и се споделя между всички GPU консуматори.

**б) Node residency предпочитания (Вариант C):**
- Нодовете **ДЕКЛАРИРАТ** какво представяне искат (GPU текстура или CPU
  QImage) — **не** са отделни GPU/CPU типове нодове и **нямат** config
  toggle.
- Frame обектът предоставя lazy кешираните представяния; транспортът
  конвертира автоматично на границата.
- GPU нод (`VideoEffectNode`/`VideoOutputNode`) → `asTexture()`; CPU нод
  (`FrameToTensorNode`) → `asImage()`.

**в) GPU-resident транспорт (Път B):**
- Upload веднъж при първия GPU нод (lazy); текстурите текат между GPU
  нодовете **без CPU↔GPU пресичания**; display консумира текстурата
  директно (zero-copy); readback само при CPU консуматор на границата.
- Fan-out: всяко представяне се изчислява **най-много веднъж на кадър**,
  споделено между всички консуматори от същия тип.

**г) Qt6 първо, Qt5 после:**
- GPU-resident транспортът се имплементира **първо на Qt6**
  (`QVideoFrame::fromRhiTexture` нативно), верифицира се, после се портва
  на Qt5 (custom `QAbstractVideoBuffer` с GL texture handle).

**д) Формат:**
- NV12 от декодера → 2 текстури (Y+UV) → първият шейдър прави YUV→RGB +
  ефект → RGBA текстура → display семплира RGBA (без конверсия).

## Acceptance Criteria

- [x] 1. **Lazy кешове.** `VideoFrameData` с lazy `asImage()` (CPU QImage
       кеш) + lazy `asTexture()` (GPU текстура кеш) — всяко представяне се
       изчислява най-много веднъж на кадър и се споделя.
       **CPU частта (asImage) е имплементирана** (2026-08-26); **GPU частта
       (asTexture/fromTexture/isGpuResident) е имплементирана** (Stage 2A,
       2026-08-26) — lazy upload при първия GPU консуматор, кеширан handle,
       споделен между всички GPU консуматори.
- [x] 2. **Display/processing разделение.** Display = `frame()` zero-copy;
       processing = `asImage()` 1×/кадър споделено.
       **CPU частта е имплементирана** — `VideoEffectNode` CPU път и
       `VideoOutputNode` software/consumer път ползват `asImage()`.
- [x] 3. **Fan-out.** N консуматора споделят една конверсия (не N×
       `toImage()`); всяко представяне се изчислява най-много веднъж на
       кадър, споделено между всички консуматори от същия тип.
       **CPU частта е имплементирана** — кешът е в споделения
       `VideoFrameData` обект; консуматорите (VideoEffectNode +
       VideoOutputNode) споделят една конверсия.
- [ ] 4. **Node residency предпочитания (Вариант C).** Нодовете ДЕКЛАРИРАТ
       какво представяне искат (GPU текстура или CPU QImage) — не са
       отделни GPU/CPU типове нодове и нямат config toggle. GPU нод
       (`VideoEffectNode`/`VideoOutputNode`) → `asTexture()`; CPU нод
       (`FrameToTensorNode`) → `asImage()`. Транспортът конвертира
       автоматично на границата.
       **Частично (Stage 2B):** `VideoEffectNode` GPU пътят ползва
       `asTexture()` (текстура-вход) и извежда `VideoFrameData::fromTexture()`
       (текстура-изход); `VideoOutputNode` GL blit пътят консумира
       GPU-resident RGBA текстурата директно (`presentTexture`). Остава:
       `FrameToTensorNode` → `asImage()` (частен REQ-AI-007).
- [ ] 5. **GPU-resident транспорт (Път B).** Upload веднъж при първия GPU
       нод (lazy); текстурите текат между GPU нодовете без CPU↔GPU
       пресичания; display консумира текстурата директно (zero-copy);
       readback само при CPU консуматор на границата.
       **Частично (Stage 2B):** ефект веригата е изцяло GPU-resident —
       `asTexture()` (lazy upload веднъж) → `processTexture()` (bind, без
       upload/readback) → `fromTexture()` (RGBA текстура) → вторият ефект
       консумира текстурата директно → GL blit display консумира текстурата
       директно (`presentTexture`, zero-copy). Qt6 native display
       (QVideoWidget) прави readback на границата (`presentableFrame` →
       `asImage()`) — Stage 2C ще презентира текстурата директно.
- [ ] 6. **Qt6 първо, Qt5 после.** GPU-resident транспортът се имплементира
       първо на Qt6 (`QVideoFrame::fromRhiTexture` нативно), верифицира се,
       после се портва на Qt5 (custom `QAbstractVideoBuffer` с GL texture
       handle).
       **Отклонение (Stage 2A/2B):** имплементирано едновременно на Qt5+Qt6
       през споделения GL контекст (`VideoGLContextManager` + `asTexture`/
       `fromTexture` с raw GL texture handle-и) вместо `fromRhiTexture` —
       същият zero-copy ефект, без Qt6-only API. `fromRhiTexture` остава
       бъдещ лост за native Qt6 display.
- [ ] 7. **Формат.** NV12 от декодера → 2 текстури (Y+UV) → първият шейдър
       прави YUV→RGB + ефект → RGBA текстура → display семплира RGBA (без
       конверсия).
       **Частично (Stage 2B):** NV12 → 2 текстури (Y+UV) → `processTexture()`
       YUV→RGB + ефект → RGBA текстура → GL blit display семплира RGBA
       директно (`presentTexture`). Qt6 native display прави readback на
       границата (Stage 2C).
- [ ] 8. **Фаза 1.** `ImageData` остава; новите нодове работят редом без да
       чупят съществуващите графи.
- [ ] 9. **Фаза 2.** Еквивалентност доказана — ръчна оценка от потребителя
       (визуално; без пиксел-диф харнес).
- [x] 10. **Фаза 3.** `VideoTransformNode`, `FrameToTensorNode`,
        `VideoOutputNode` fallback, saved графи мигрирани.
        **Имплементирано (2026-08-26):** `VideoTransformNode` е премахнат
        (заменен от `VideoEffectNode`, който покрива всичките му операции);
        `ImageData` типът е изтрит — единственият frame тип е
        `VideoFrameData`; image портовете са премахнати от source-ите и
        `VideoOutputNode`; `FrameToTensorNode` мигрира към `VideoFrameData`
        (частен REQ-AI-007); saved-graph последиците са документирани в
        README-а.
- [x] 11. **Qt5 + Qt6 builds PASS.**
- [ ] 12. **Тестове** (отложени по стояща инструкция).

## Проследимост

- **Коммити:** `3f9ec84` (feat: lazy asImage() QImage cache в VideoFrameData),
  `093b557` (refactor: консуматорите ползват asImage()) — CPU частта на AC
  1/2/3; `6cb1aaa` (feat: shared GL context manager VideoGLContextManager),
  `803e4f6` (feat: VideoFrameData texture transport asTexture/fromTexture/
  isGpuResident), `9a39006` (refactor: VideoEffectGLProcessor uses shared GL
  context) — Stage 2A GPU-resident транспорт; `b6af20e` (feat:
  VideoEffectGLProcessor processTexture — texture output, no readback),
  `d1a5e7a` (feat: VideoEffectNode texture in/out + GL blit presentTexture +
  DAQSTER_AUTOSTART_EFFECT2) — Stage 2B ефект верига;
  `d4a90ee` (fix: nodeeditor onNodeDataArrived + dataArrivalChangesGeometry —
  scene invalidation fix), `8418f53` (fix: CustomDataFlowScene override + 3
  video nodes opt-out — scene repaint-only optimization)
- **Код:** `src/plugins/common/NodeDataTypes/VideoFrameData.h` (asImage +
  m_imageCache + asTexture/fromTexture/isGpuResident/isGpuRgba),
  `src/plugins/common/GL/VideoGLContextManager.h` (споделен GL контекст),
  `src/plugins/demo_nodeditor_nodes/Sources/Video/` (VideoEffectGLProcessor.
  {h,cpp} processTexture, VideoEffectNode.cpp texture in/out, VideoGLBlitWidget.
  {h,cpp} presentTexture, VideoOutputNode.cpp GpuRgba display, VideoGLShaders.h
  buildRgbaEffectFragmentSource), `src/plugins/node_editor_ide/
  NodeEditorIdeObject.cpp` (DAQSTER_AUTOSTART_EFFECT2)
- **Документация:** дизайн документ `video-frame-consolidation-design.md` §3.1,
  §3.6, §4; статус `2026-08-24-status.md` §4–§14
- **Тестове:** отложени (standing instruction)

## Бележки по имплементацията (план)

- **Един тип `VideoFrameData`:** QVideoFrame payload + lazy QImage кеш
  (`asImage()`) + lazy GL текстура кеш (`asTexture()`).
- **„Най-много едно копие на представяне на кадър, lazy и споделено“** —
  конверсията става веднъж, резултатът се споделя между консуматорите.
- **Node residency предпочитания (Вариант C):** нодовете декларират какво
  представяне искат (GPU текстура / CPU QImage); транспортът конвертира
  автоматично на границата; без config toggle.
- **GPU-resident транспорт (Път B):** upload веднъж при първия GPU нод;
  текстурите текат между GPU нодовете без CPU↔GPU пресичания; readback
  само при CPU консуматор на границата.
- **Qt6 първо, Qt5 после:** GPU-resident транспортът се имплементира първо
  на Qt6 (`QVideoFrame::fromRhiTexture`), после се портва на Qt5 (custom
  `QAbstractVideoBuffer` с GL texture handle).
- **Формат:** NV12 → 2 текстури (Y+UV) → първият шейдър YUV→RGB + ефект →
  RGBA текстура → display семплира RGBA.
- **Zero-copy само в рамките на един домейн** — GPU→CPU копие е неизбежно
  при processing.
- **Фаза 1:** новите нодове (PL-028, PL-029, PL-030, PL-031) се добавят
  паралелно; `ImageData` остава.
- **Фаза 2:** еквивалентност — ръчна оценка от потребителя (решение
  2026-08-24); без пиксел-диф харнес.
- **Фаза 3:** миграция на `VideoTransformNode`, `FrameToTensorNode` (частен
  REQ-AI-007), `VideoOutputNode` fallback, saved графи (type id промени).

## Бележки по имплементацията (актуално, 2026-08-26 — Фаза 3)

- **`VideoTransformNode` премахнат** (комит `688c899`): registry
  `registerModel<VideoTransformNode>("Video")` и CMake записите са изтрити,
  файловете `VideoTransformNode.{h,cpp}` са изтрити. `VideoTransformOps.{h,cpp}`
  и `OpenCVTransforms.cpp` **остават** — `VideoEffectOps` ги ползва за CPU
  ефектите (brightness/contrast/grayscale/invert/sepia/channelSwap/flip/blur +
  gaussianBlur/canny/threshold при HAVE_OPENCV).
- **`ImageData` типът изтрит** (комит `817002e`):
  `src/plugins/common/NodeDataTypes/ImageData.h` е изтрит; grep за `ImageData`
  в `src/` и `tests/` → 0 резултата. Единственият frame тип е `VideoFrameData`.
- **Image портовете премахнати:** source-ите (`CameraSourceNode`,
  `VideoFileSourceNode`, `StreamSourceNode`) емитират само `VideoFrameData`
  (port 0) + `SampledData` (port 1, audio); `VideoOutputNode` приема само
  `VideoFrameData` (комити `63010f3`, `3fc51a3`).
- **Saved-graph последици:** стари графи с `"VideoTransform"` registry ключ или
  image edges **няма да се заредят** — пресвържете към `VideoEffect` +
  `VideoFrameData` вериги. Документирано в README-а на demo_nodeditor_nodes.
- **Фаза 1 (AC 8) е завършена исторически** — `ImageData` остана през Фаза 1,
  новите нодове работеха паралелно; **Фаза 2 (AC 9)** чака ръчната визуална
  оценка от потребителя.