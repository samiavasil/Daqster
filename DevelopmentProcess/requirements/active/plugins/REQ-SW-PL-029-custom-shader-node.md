# REQ-SW-PL-029: CustomShaderNode — общ GPU compute нод с pluggable адаптери

- **Статус:** ACTIVE
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

## Acceptance Criteria

- [ ] 1. **Типова съвместимост (v1).** `CustomShaderNode` приема
       `VideoFrameData`, извежда `VideoFrameData`.
- [ ] 2. **`IDataAdapter` интерфейс.** Pluggable адаптери: upload/download
       между data и текстура.
- [ ] 3. **Потребителски GLSL + uniform-и.** Шаблон + uniform стойности
       (време, слайдери).
- [ ] 4. **Рантайм компилация с обработка на грешки.** Компилационни грешки
       → потребителски съобщения, без crash.
- [ ] 5. **Qt5 + Qt6 builds PASS.**
- [ ] 6. **Тестове** (отложени по стояща инструкция).

## Проследимост

- **Коммити:** чака имплементация
- **Код:** чака имплементация
- **Документация:** дизайн документ `video-frame-consolidation-design.md` §3.3;
  статус `2026-08-24-status.md` §13
- **Тестове:** отложени (standing instruction)

## Бележки по имплементацията (план)

- **`IDataAdapter`:** интерфейс за upload/download между data и текстура;
  v1 видео адаптер, v2+ SampledData/DAQ и TensorData адаптери.
- **Шаблон + uniform-и:** потребителски GLSL код + uniform стойности (време,
  слайдери) — UI за задаване.
- **Рантайм компилация:** `addShaderFromSourceCode()`; грешките се показват
  на потребителя, не crash-ват приложението.