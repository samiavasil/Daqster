# RDD-STATUS.md — Състояние на фазите (Requirements-Driven Development)

Текущото състояние на фазите и изискванията в RDD процеса. **Новите сесии четат
този файл първо** (AGENTS.md → Session Restore сочи насам), за да възстановят
контекста и да знаят какво предстои.

## Фаза 1 & 2 — Requirements Manager Core (REQ-SW-PL-001..008)

**Статус: DONE** ✅

Имплементирани и верифицирани. Qt5 + Qt6 builds PASS; 28 unit теста green.

## Фаза 3 — Graph & Traceability (REQ-SW-PL-009, REQ-SW-PL-010)

**Статус: DONE** ✅

- `REQ-SW-PL-009` — Interactive Dependency Graph Viewer: **DONE** ✅
- `REQ-SW-PL-010` — Traceability Matrix View & Export: **DONE** ✅

## Фаза 3.5 — Auto-Layout (REQ-SW-PL-016)

**Статус: DONE** ✅ (2026-08-03)

## Фаза 3.6 — Multi-Repo Requirements View (REQ-SW-PL-012)

**Статус: DONE** ✅ (2026-08-07)

## Фаза 3.7 — Requirements Search Engine (REQ-SW-PL-011)

**Статус: DONE** ✅ (2026-08-07)

## Фаза 3.8 — Phase Status Indicators (REQ-SW-PL-017)

**Статус: DONE** ✅ (2026-08-07)

## Видео пайплайн — Video Nodes (REQ-SW-PL-018, REQ-SW-PL-019)

**Статус: DONE** ✅ (2026-08-07)

## Фаза 3.9 — Zero-Copy Video Frame Transport (REQ-SW-PL-020)

**Статус: DONE** ✅ (2026-08-07)

## Фаза 3.10 — Unified Sampled Data Transport (REQ-SW-PL-022)

**Статус: DONE** ✅ (имплементация 2026-08-09; тестове 2026-08-11)

## Фаза 3.11 — DAQ Display Multi-Plot v1 (REQ-SW-PL-023)

**Статус: DONE** ✅ (2026-08-10)

## Фаза 3.12 — AudioSource SampledData миграция (REQ-SW-PL-024)

**Статус: DONE** ✅ (2026-08-10)

## Фаза 3.13 — DAQ Display Multi-Plot v2 (REQ-SW-PL-025)

**Статус: DONE** ✅ (2026-08-11)

## Фаза 3.14 — Video Frame Consolidation (REQ-SW-PL-028, PL-029, PL-030, PL-032)

**Статус: Имплементирано (бранч `feat/REQ-SW-PL-028-032-video-frame-cache-combo`; не merge-нато в develop)**

- **REQ-SW-PL-028** — `VideoEffectNode` (един нод с комбобокс, GPU/CPU backend):
  7 deprecated alias-а премахнати (`dd82f4e`); blur + OpenCV добавени (`095981b`).
  AC 1–6 `[x]`. Тестове отложени.
- **REQ-SW-PL-029** — `CustomShaderNode` (общ GPU compute нод с runtime GLSL):
  mainImage contract, рантайм компилация, YUV→RGBA pre-pass, texture() compat fix.
  AC 1–7 `[x]`. Комити: `42ba334`, `0682c1b`. **DONE** ✅
- **REQ-SW-PL-030** — `FrameSamplerNode` (every-N / max-fps, zero-copy passthrough).
  Тестове отложени.
- **REQ-SW-PL-032** — консолидация: lazy `asImage()` + `asTexture()` кешове,
  GPU-resident ефект верига (Stage 2A/2B), Фаза 3 завършена (2026-08-26):
  `VideoTransformNode` премахнат, `ImageData` изтрит, image портовете премахнати.
  Scene invalidation fix (2026-08-27): `d4a90ee`, `8418f53`.
  AC 1–3, 10–11 `[x]`; AC 4–7 частично; AC 9 (Фаза 2 еквивалентност) чака
  ръчна оценка от потребителя. Тестове отложени.
- **REQ-SW-PL-034** — `VideoOutputNode` вградени ефекти (опционални, default
  none): комбобокс с „No effect" + `QStackedWidget` параметърни страници;
  ефект блокът в `setInData()` се пропуска изцяло при липса на ефект
  (zero-copy passthrough byte-identical); GPU path (`processTexture`) при
  хардуерен GL, CPU path/fallback (`cpuApply`); `save()`/`load()` с `"effect"`
  id + параметри, backward compatible. Комит: `615f53e`. AC 1–5 `[x]`.
  Тестове: `demo_nodeditor_videooutput_tests` 10/10 (Qt5 + Qt6).

## Състояние на имплементацията

| REQ | Имплементация |
|-----|---------------|
| REQ-SW-PL-020 | Имплементирано |
| REQ-SW-PL-021 | Частично (AC 1-3); AC 4/6/7 отворени |
| REQ-SW-PL-022 | Имплементирано; nPorts fix (`d5145c2`) поправи AC 8 |
| REQ-SW-PL-023 | Имплементирано |
| REQ-SW-PL-024 | Имплементирано |
| REQ-SW-PL-025 | Имплементирано |
| REQ-SW-PL-026 | Имплементирано |
| REQ-SW-PL-027 | Имплементирано |
| REQ-SW-PL-028 | Имплементирано (AC 1–6 `[x]`, тестове отложени) |
| REQ-SW-PL-029 | **Имплементирано (DONE)** — `42ba334`, `0682c1b` |
| REQ-SW-PL-030 | Имплементирано (тестове отложени) |
| REQ-SW-PL-031 | Неимплементирано |
| REQ-SW-PL-032 | Имплементирано — Фаза 3 завършена; AC 1–3, 10–11 `[x]`; AC 9 чака |
| REQ-SW-PL-034 | Имплементирано (AC 1–5 `[x]`) — `615f53e`; тестове 10/10 (Qt5 + Qt6) |
| REQ-SW-FW-008 | Имплементирано |
| REQ-SW-FW-007 | Бъдещ лост (roadmap) |

## Известни неувързаности

| ID | REQ | Описание | Статус | Митигация/Подход |
|----|-----|----------|--------|------------------|
| INC-001 | REQ-SW-PL-021 | PL-021 AC 2 е `[x]`, но texture-handle пътят е документиран като неимплементиран | Отворено | — |
| INC-002 | REQ-SW-PL-022/023/024/025 | REQ файловете остават ACTIVE в `active/`, докато RDD-STATUS ги води DONE | Отворено | — |
| INC-003 | — | EmbeddingData няма консуматор | Отворено | — |
| INC-004 | — | Qt5 преномерацията на image порта — **Затворено** (2026-08-26) | Затворено | — |
| INC-005 | REQ-SW-PL-020 | Фаза 1 GPU flip е флипва вертикално (u_flipY) | Отворено | — |
| INC-006 | REQ-SW-PL-022 | nPorts off-by-one: audio Sample port беше недостъпен на индекс 2 | **Затворено (2026-08-27)** — поправен в `d5145c2` | — |
| INC-007 | REQ-SW-PL-033 | Qt6 камера аудио capture липсва — CameraSourceNode sample порт емитира invalid на Qt6 (Qt платформена лимитация: QAudioBufferOutput е playback-only, QAudioProbe е премахнат) | Отворен | FFmpeg аудио capture + device matching |
