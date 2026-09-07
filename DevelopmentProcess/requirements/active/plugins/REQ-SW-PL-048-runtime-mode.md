# REQ-SW-PL-048: Runtime режим на приложението (node diagram = application)

- **Статус:** ACTIVE
- **Приоритет:** High
- **Отговорник (роля):** Ivan (Implementation)
- **Дата:** 2026-09-07
- **Родител:** REQ-SW-PL-014
- **Зависи от:** REQ-SW-PL-049, REQ-SW-PL-050

## Описание

Daqster се стартира с параметър `--run <flow.flow>`, зарежда flow-а и тръгва в
runtime режим: node editor-ът е скрит, а деембеднатите виджети на нодовете
формират UI-то на приложението. В editor-а се добавя presentation-mode toggle
(като Pure Data): скрива canvas-а, показва само деембеднатите виджети.
Визия: "създаваш си нод диаграмата — и имаш приложението".

Деембедингът е ключовият механизъм — той е мостът между editor и приложение
(моделът на LabVIEW front panel / Pure Data presentation mode / TouchDesigner
perform mode). Архитектурното предложение е в
`docs/Architecture/runtime-mode-architecture.md`.

## Acceptance Criteria

- [ ] 1. `Daqster --run <file.flow>` стартира runtime режим без editor canvas
- [ ] 2. Деембеднатите виджети се показват като UI на приложението (според ui
       секцията на flow-а)
- [ ] 3. Presentation toggle в editor-а скрива canvas-а и показва деембеднатите
       виджети
- [ ] 4. Съществува generic auto-start механизъм за нодовете (не само
       video-specific `startVideoPlayback()`)
- [ ] 5. Затварянето на runtime режима е чисто (thread-safe, без crash)
- [ ] 6. Flow файлът се валидира преди старт в runtime режим
- [ ] 7. Тестове (отложени по текущата инструкция)

## Проследимост

- **Коммити:** (pending commit)
- **Код:** `src/apps/Daqster/` (CLI `--run`), `src/plugins/node_editor_ide/`
  (presentation toggle, runtime shell), `src/plugins/demo_nodeditor_nodes/`
  (generic auto-start)

## Бележка

Изискването е създадено по решение на потребителя (2026-09-07) след
проучването на SDRangel (workspace/MDI модел, core/gui split) и одита на
текущото състояние в `docs/Architecture/runtime-mode-architecture.md`.
Фаза 1 от трифазния план (Фаза 2: headless — REQ-SW-PL-051; Фаза 3:
дистрибуция — REQ-SW-PL-052). Unit тестове са ОТЛОЖЕНИ по стоящата инструкция
„НОВИ ТЕСТОВЕ СТОП".