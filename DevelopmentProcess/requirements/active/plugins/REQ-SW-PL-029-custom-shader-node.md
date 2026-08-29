# REQ-SW-PL-029: CustomShaderNode — общ GPU compute нод с pluggable адаптери

- **Статус:** DONE
- **Приоритет:** P1
- **Отговорник (роля):** Ivan (Implementation)
- **Дата:** 2026-08-24
- **Родител:** REQ-SW-PL-028 (active/plugins/REQ-SW-PL-028-video-effect-node.md)
- **Зависи от:** REQ-SW-PL-028 (VideoEffectNode), REQ-SW-PL-020 (active/plugins/REQ-SW-PL-020-zero-copy-video-frame-display.md)

## Описание

`CustomShaderNode` е **общ GPU compute нод** с pluggable data-адаптери
(`IDataAdapter`): `data → текстура → шейдър → текстура → data`
(upload → compute → download).

1. **v1:** видео (`VideoFrameData`).
2. **v2+:** `SampledData`/DAQ, `TensorData`.
3. **Шаблон + uniform-и** — потребителят задава GLSL код + uniform стойности
   (време, слайдери).
4. **Рантайм компилация** (`addShaderFromSourceCode`) + **обработка на
   грешки** — компилационни грешки → потребителски съобщения, не crash.

**Потвърждение (решение 2026-08-25):** `CustomShaderNode` остава **отделен
нод** — не се вгражда в `VideoEffectNode`. Обосновка:

- **Различно UI:** GLSL редактор + compile + error log (power-user функция)
  — не е комбобокс с параметри като `VideoEffectNode`.
- **Разширяемост:** общ GPU compute нод с data-адаптери — v2+ към
  `SampledData`/DAQ, `TensorData`; `VideoEffectNode` е ограничен до видео
  ефекти.

## Acceptance Criteria

- [x] 1. **Отделен нод (потвърждение).** `CustomShaderNode` е отделен нод —
       не вграден в `VideoEffectNode` (различно UI: GLSL редактор + compile
       + error log; power-user функция; разширяем към DAQ/други типове).
- [x] 2. **Типова съвместимост (v1).** `CustomShaderNode` приема
       `VideoFrameData`, извежда `VideoFrameData`.
- [x] 3. **`IDataAdapter` интерфейс.** Pluggable адаптери: upload/download
       между data и текстура.
- [x] 4. **Потребителски GLSL + uniform-и.** Шаблон + uniform стойности
       (време, слайдери).
- [x] 5. **Рантайм компилация с обработка на грешки.** Компилационни грешки
       → потребителски съобщения, без crash.
- [x] 6. **Qt5 + Qt6 builds PASS.**
- [x] 7. **Тестове** (отложени по стояща инструкция).

## Проследимост

- **Коммити:** `42ba334` (feat: CustomShaderNode — runtime GLSL shader with
  mainImage contract), `0682c1b` (fix: detect GL profile and use texture() or
  texture2D() accordingly — texture() compat fix)
- **Код:** `src/plugins/demo_nodeditor_nodes/Sources/Video/CustomShaderNode.{h,cpp}`
  (node model + UI), `CustomShaderGLProcessor.{h,cpp}` (GPU processor:
  runtime GLSL compile, program cache, YUV→RGBA pre-pass), `CustomShaderWidget.{h,cpp}`
  (GLSL editor + compile button + error log + uniform controls)
- **Документация:** дизайн документ `video-frame-consolidation-design.md` §3.3;
  статус `2026-08-24-status.md` §13
- **Тестове:** отложени (standing instruction)

## Бележки по имплементацията (план)

- **Отделен нод (потвърдено 2026-08-25):** не се вгражда в
  `VideoEffectNode` — различно UI (GLSL редактор + compile + error log),
  power-user функция, разширяем към DAQ/други типове (общ GPU compute нод
  с data-адаптери).
- **`IDataAdapter`:** интерфейс за upload/download между data и текстура;
  v1 видео адаптер, v2+ SampledData/DAQ и TensorData адаптери.
- **Шаблон + uniform-и:** потребителски GLSL код + uniform стойности (време,
  слайдери) — UI за задаване.
- **Рантайм компилация:** `addShaderFromSourceCode()`; грешките се показват
  на потребителя, не crash-ват приложението.