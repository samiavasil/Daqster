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

- [ ] 1. **Lazy кешове.** `VideoFrameData` с lazy `asImage()` (CPU QImage
       кеш) + lazy `asTexture()` (GPU текстура кеш) — всяко представяне се
       изчислява най-много веднъж на кадър и се споделя.
- [ ] 2. **Display/processing разделение.** Display = `frame()` zero-copy;
       processing = `asImage()` 1×/кадър споделено.
- [ ] 3. **Fan-out.** N консуматора споделят една конверсия (не N×
       `toImage()`); всяко представяне се изчислява най-много веднъж на
       кадър, споделено между всички консуматори от същия тип.
- [ ] 4. **Node residency предпочитания (Вариант C).** Нодовете ДЕКЛАРИРАТ
       какво представяне искат (GPU текстура или CPU QImage) — не са
       отделни GPU/CPU типове нодове и нямат config toggle. GPU нод
       (`VideoEffectNode`/`VideoOutputNode`) → `asTexture()`; CPU нод
       (`FrameToTensorNode`) → `asImage()`. Транспортът конвертира
       автоматично на границата.
- [ ] 5. **GPU-resident транспорт (Път B).** Upload веднъж при първия GPU
       нод (lazy); текстурите текат между GPU нодовете без CPU↔GPU
       пресичания; display консумира текстурата директно (zero-copy);
       readback само при CPU консуматор на границата.
- [ ] 6. **Qt6 първо, Qt5 после.** GPU-resident транспортът се имплементира
       първо на Qt6 (`QVideoFrame::fromRhiTexture` нативно), верифицира се,
       после се портва на Qt5 (custom `QAbstractVideoBuffer` с GL texture
       handle).
- [ ] 7. **Формат.** NV12 от декодера → 2 текстури (Y+UV) → първият шейдър
       прави YUV→RGB + ефект → RGBA текстура → display семплира RGBA (без
       конверсия).
- [ ] 8. **Фаза 1.** `ImageData` остава; новите нодове работят редом без да
       чупят съществуващите графи.
- [ ] 9. **Фаза 2.** Еквивалентност доказана — ръчна оценка от потребителя
       (визуално; без пиксел-диф харнес).
- [ ] 10. **Фаза 3.** `VideoTransformNode`, `FrameToTensorNode`,
       `VideoOutputNode` fallback, saved графи мигрирани.
- [ ] 11. **Qt5 + Qt6 builds PASS.**
- [ ] 12. **Тестове** (отложени по стояща инструкция).

## Проследимост

- **Коммити:** чака имплементация
- **Код:** чака имплементация
- **Документация:** дизайн документ `video-frame-consolidation-design.md` §3.1,
  §4; статус `2026-08-24-status.md` §4–§14
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