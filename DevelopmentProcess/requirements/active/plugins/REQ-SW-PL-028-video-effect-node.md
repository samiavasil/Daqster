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

1. **Един нод с комбобокс (рефакторинг, решение 2026-08-25)** — вместо 7
   отделни subclass-а (един нод = един ефект), **ЕДИН `VideoEffectNode` с
   комбобокс** за избор на ефект + параметри/конфигурация за избрания
   (като VideoTransformNode / Image пътя: комбо + QStackedWidget).
2. **GPU backend** — когато има хардуерен GL. Детекцията различава
   хардуерен GL от софтуерните рендерери (`llvmpipe` / `softpipe` /
   `SwiftShader`).
3. **CPU backend** — когато няма хардуерен GL, или ефектът е CPU-only.
4. **CPU-only ефекти** вървят на CPU **навсякъде** (независимо от GL
   наличност).
5. **`EffectSpec`** — описание на ефекта с опционален GLSL (за GPU backend).
   EffectSpec-ите остават в `VideoEffectOps.h/.cpp` (7-те ефекта с CPU fn +
   GLSL body); комбобоксът избира между тях — не се създават нови типове
   нодове.

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
- [x] 4. **Един нод с комбобокс (рефакторинг).** Един `VideoEffectNode` с
       комбобокс за избор на ефект + параметри/конфигурация за избрания
       (като VideoTransformNode / Image пътя: комбо + QStackedWidget).
       Вместо 7 отделни subclass-а (един нод = един ефект). 7-те стари
       subclass-а бяха **премахнати на 2026-08-26** (решение на потребителя)
       — стари saved графи с alias registry ключове вече не се зареждат;
       единственият регистриран ефект нод е `VideoEffect`.
- [x] 5. **EffectSpec-ите остават.** `VideoEffectOps.h/.cpp` — 7-те ефекта с
       CPU fn + GLSL body; комбобоксът избира между тях (не се създават
       нови типове нодове).
- [x] 6. **Qt5 + Qt6 builds PASS.**
- [ ] 7. **Тестове** (отложени по стояща инструкция).

## Проследимост

- **Коммити:** `54a3162` (VideoGLShaders.h), `c0ca1b1` (VideoEffectOps),
  `8b0c353` (VideoEffectGLProcessor), `dc1d11f` (VideoEffectNode + 7
  subclass-а), `a4111be` (CMake + регистрация), `3e91e58`/`1aa23c2`
  (autostart smoke driver); **комбобокс рефакторинг (AC 4/5):** `e84f6d0`
  (един VideoEffectNode с комбо + QStackedWidget + 7 deprecated aliases),
  `42bb57a` (регистрация на VideoEffectNode преди aliases-ите), `9fb46b9`
  (smoke driver през load()); **премахване на aliases-ите (2026-08-26):**
  `dd82f4e` (7-те deprecated alias нода премахнати — единствен `VideoEffect`)
- **Код:** `src/plugins/demo_nodeditor_nodes/Sources/Video/` (VideoGLShaders.h,
  VideoEffectOps.{h,cpp}, VideoEffectGLProcessor.{h,cpp}, VideoEffectNode.{h,cpp})
- **Документация:** дизайн документ `video-frame-consolidation-design.md` §3.2;
  статус `2026-08-24-status.md` §10; `docs/plugins/demo_nodeditor_nodes/README.md`
- **Тестове:** отложени (standing instruction)

## Бележки по имплементацията (план)

- **Един нод с комбобокс (рефакторинг):** от 7-те thin subclass-а към един
  `VideoEffectNode` с комбобокс за избор на ефект + QStackedWidget с
  параметри/конфигурация за избрания ефект (като VideoTransformNode /
  Image пътя). Регистрацията става веднъж (един нод тип), ефектът се
  избира от комбобокса.
- **EffectSpec-ите остават:** `VideoEffectOps.h/.cpp` — 7-те ефекта с CPU fn
  + GLSL body; не се създават нови типове нодове.
- **Backend избор:** runtime-решение по ефект + GL детекция. Детекцията на
  хардуерен GL различава `llvmpipe`/`softpipe`/`SwiftShader` (софтуерни
  рендерери) от реален хардуерен GL — `QOpenGLContext` capabilities /
  renderer стринг.
- **`EffectSpec`:** описание на ефекта — име, параметри, опционален GLSL за
  GPU backend-а.
- **CPU-only ефекти:** винаги CPU, независимо от GL — не се опитва GPU път.
- **Staged:** нодът се добавя във Фаза 1 (паралелно с `ImageData`); замества
  `VideoTransformNode` във Фаза 3 (виж REQ-SW-PL-032).