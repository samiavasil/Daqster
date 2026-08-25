# REQ-SW-PL-028: VideoEffectNode — GPU/CPU backend по ефект

- **Статус:** ACTIVE
- **Приоритет:** P1
- **Отговорник (роля):** Ivan (Implementation)
- **Дата:** 2026-08-24
- **Родител:** REQ-SW-PL-018 (архив: archive/plugins/REQ-SW-PL-018-video-source-and-processing-nodes.md)
- **Зависи от:** REQ-SW-PL-020 (active/plugins/REQ-SW-PL-020-zero-copy-video-frame-display.md)

## Описание

`VideoEffectNode` е видео ефект нод с **runtime-избран backend по ефект** —
един нод решава в рантайм дали даденият ефект върви на GPU или CPU:

1. **Един нод = един ефект** — регистрация по ефект; всеки ефект е отделна
   инстанция на нода.
2. **GPU backend** — когато има хардуерен GL. Детекцията различава
   хардуерен GL от софтуерните рендерери (`llvmpipe` / `softpipe` /
   `SwiftShader`).
3. **CPU backend** — когато няма хардуерен GL, или ефектът е CPU-only.
4. **CPU-only ефекти** вървят на CPU **навсякъде** (независимо от GL
   наличност).
5. **`EffectSpec`** — описание на ефекта с опционален GLSL (за GPU backend).

Нодът приема `VideoFrameData` и извежда `VideoFrameData` — работи върху
frame-а; не тригерира lazy `asImage()` освен ако ефектът не е CPU-only.

**Staged:** нодът се добавя във Фаза 1 (паралелно с `ImageData`) и замества
`VideoTransformNode` във Фаза 3 (виж REQ-SW-PL-032).

## Acceptance Criteria

- [x] 1. **Типова съвместимост.** `VideoEffectNode` приема `VideoFrameData`
       (type id `"video-frame"`), извежда `VideoFrameData`.
- [x] 2. **Runtime backend избор.** Backend-ът се избира в рантайм по ефект +
       GL детекция; при липса на хардуерен GL (`llvmpipe`/`softpipe`/
       `SwiftShader`) се ползва CPU.
- [x] 3. **CPU-only ефекти.** CPU-only ефекти вървят на CPU на всички
       платформи (независимо от GL наличност). Механизмът е имплементиран
       (`EffectSpec::Backend::CpuOnly` + runtime проверка в `setInData()`);
       текущите 7 ефекта са `GpuOrCpu`.
- [x] 4. **Един нод = един ефект.** Регистрация по ефект; `EffectSpec`
       описва ефекта с опционален GLSL.
- [x] 5. **Qt5 + Qt6 builds PASS.**
- [ ] 6. **Тестове** (отложени по стояща инструкция).

## Проследимост

- **Коммити:** `54a3162` (VideoGLShaders.h), `c0ca1b1` (VideoEffectOps),
  `8b0c353` (VideoEffectGLProcessor), `dc1d11f` (VideoEffectNode + 7
  subclass-а), `a4111be` (CMake + регистрация), `3e91e58`/`1aa23c2`
  (autostart smoke driver)
- **Код:** `src/plugins/demo_nodeditor_nodes/Sources/Video/` (VideoGLShaders.h,
  VideoEffectOps.{h,cpp}, VideoEffectGLProcessor.{h,cpp}, VideoEffectNode.{h,cpp})
- **Документация:** дизайн документ `video-frame-consolidation-design.md` §3.2;
  статус `2026-08-24-status.md` §10; `docs/plugins/demo_nodeditor_nodes/README.md`
- **Тестове:** отложени (standing instruction)

## Бележки по имплементацията (план)

- **Backend избор:** runtime-решение по ефект + GL детекция. Детекцията на
  хардуерен GL различава `llvmpipe`/`softpipe`/`SwiftShader` (софтуерни
  рендерери) от реален хардуерен GL — `QOpenGLContext` capabilities /
  renderer стринг.
- **`EffectSpec`:** описание на ефекта — име, параметри, опционален GLSL за
  GPU backend-а.
- **CPU-only ефекти:** винаги CPU, независимо от GL — не се опитва GPU път.
- **Staged:** нодът се добавя във Фаза 1 (паралелно с `ImageData`); замества
  `VideoTransformNode` във Фаза 3 (виж REQ-SW-PL-032).