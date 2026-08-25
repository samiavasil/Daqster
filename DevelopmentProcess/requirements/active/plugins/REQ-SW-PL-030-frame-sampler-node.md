# REQ-SW-PL-030: FrameSampler — ресемплиране на video кадри

- **Статус:** ACTIVE
- **Приоритет:** P2
- **Отговорник (роля):** Ivan (Implementation)
- **Дата:** 2026-08-24
- **Родител:** REQ-SW-PL-018 (архив: archive/plugins/REQ-SW-PL-018-video-source-and-processing-nodes.md)
- **Зависи от:** REQ-SW-PL-020 (active/plugins/REQ-SW-PL-020-zero-copy-video-frame-display.md)

## Описание

`FrameSampler` е **отделен нод за ресемплиране** на video кадри:

1. **Всеки N-ти / max fps** (избираемо).
2. **Zero-copy, fan-out** — ресемплираният кадър се споделя между
   консуматорите (не се копира).
3. **Ортогонален на шейдърите** — не зависи от тях.
4. **Взаимодействие с lazy конверсията** — работи върху frame-а, не върху
   image-а; не тригерира `asImage()`.

## Acceptance Criteria

- [x] 1. **Типова съвместимост.** `FrameSampler` приема `VideoFrameData`,
       извежда `VideoFrameData`.
- [x] 2. **Режими.** Всеки N-ти кадър, max fps (избираемо).
- [x] 3. **Zero-copy.** Ресемплираният кадър се споделя (frame-ът се
       предава, не се копира).
- [x] 4. **Fan-out.** Ресемплираният кадър стига до N консуматора (същият
       `shared_ptr<VideoFrameData>` се предава на всички свързани консуматори).
- [x] 5. **Qt5 + Qt6 builds PASS.**
- [ ] 6. **Тестове** (отложени по стояща инструкция).

## Проследимост

- **Коммити:** `5b2f884` (FrameSamplerNode), `a4111be` (CMake + регистрация),
  `3e91e58`/`1aa23c2` (autostart smoke driver)
- **Код:** `src/plugins/demo_nodeditor_nodes/Sources/Video/FrameSamplerNode.{h,cpp}`
- **Документация:** дизайн документ `video-frame-consolidation-design.md` §3.4;
  статус `2026-08-24-status.md` §8; `docs/plugins/demo_nodeditor_nodes/README.md`
- **Тестове:** отложени (standing instruction)

## Бележки по имплементацията (план)

- **Режими:** всеки N-ти кадър / max fps — избираемо от потребителя.
- **Zero-copy:** ресемплираният кадър се споделя между консуматорите
  (fan-out), не се копира.
- **Lazy конверсия:** нодът работи върху frame-а, не върху image-а — не
  тригерира `asImage()`.