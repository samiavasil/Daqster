# REQ-SW-PL-051: Core/GUI разделение + headless изпълнение на flow

- **Статус:** ACTIVE
- **Приоритет:** Medium
- **Отговорник (роля):** Ivan (Implementation)
- **Дата:** 2026-09-07
- **Родител:** REQ-SW-PL-014
- **Зависи от:** REQ-SW-PL-048, REQ-SW-PL-050

## Описание

SDRangel-ски модел: всеки нод се разделя на core (логика, без GUI) и gui
(widget). Възможност за стартиране на flow headless (без Qt Widgets/OpenGL) —
`Daqster --run --headless <flow>`. Същият PluginInterface се ползва от GUI и
headless build; GUI factory методите връщат nullptr в headless режим. Един
source tree, два режима (като sdrangel / sdrangelsrv).

## Acceptance Criteria

- [ ] 1. Архитектурен шаблон core/gui за нодовете (документиран в
       docs/Architecture/)
- [ ] 2. Headless стартиране на flow без Qt Widgets/OpenGL
- [ ] 3. GUI factory методите връщат nullptr в headless режим
- [ ] 4. Същият flow работи с и без GUI
- [ ] 5. (Опционално) REST API за remote control — документирано като
       разширение
- [ ] 6. Документация на шаблона
- [ ] 7. Тестове (отложени по текущата инструкция)

## Проследимост

- **Коммити:** (pending commit)
- **Код:** `src/plugins/demo_nodeditor_nodes/` (core/gui split per node),
  `src/apps/Daqster/` (--headless), `src/plugins/common/capabilities/`
  (GUI factory nullptr contract), `docs/Architecture/`

## Бележка

Изискването е създадено по решение на потребителя (2026-09-07) след
проучването на SDRangel (Core/GUI split: същият PluginInterface, GUI factory-тата
връщат nullptr headless; два бинарника sdrangel/sdrangelsrv; Message/MessageQueue
за между-нишкова комуникация). Архитектурното предложение е в
`docs/Architecture/runtime-mode-architecture.md` (секция 4, Фаза 2). Unit тестове
са ОТЛОЖЕНИ по стоящата инструкция „НОВИ ТЕСТОВЕ СТОП".