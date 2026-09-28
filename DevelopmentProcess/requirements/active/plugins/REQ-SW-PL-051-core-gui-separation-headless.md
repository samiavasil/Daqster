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

- **Коммити:** 878d95d (премахване на 32 дублирани widget файла)
- **Код:** `src/plugins/demo_nodeditor_nodes_core/`,
  `src/plugins/demo_nodeditor_nodes_gui/`,
  `src/plugins/demo_nodeditor_nodes_gui/NodeWidgetFactory.cpp`,
  `src/frame_work/base/src/engine/HeadlessApp.h`

## Оставаща миграция (задължителна)

Измерено в кода на 2026-09-29. Миграцията на core/gui **не е завършена**.

Разделени са 9 от 22 модела — връщат `nullptr` от `embeddedWidget()`, widget-ът им
е в `demo_nodeditor_nodes_gui`. Липсва само свързването: `NodeWidgetFactory` има
0 места, от които се извиква.

Неразделени са 13 модела, които сами конструират widget в `_core`:

| Група | Модели | Брой |
|---|---|---|
| Празен `new QWidget()` | CameraSourceNode, CustomShaderNode, FrameSamplerNode, StreamSourceNode, VideoEffectNode, VideoFileSourceNode | 6 |
| Истински widget | AudioSourceDataModel(+Obsolete), ConsoleDataModel, LLamaModelDataModel, GamepadModel, NumberSourceDataModel | 6 |
| **Отложен** | **VideoOutputNode** | 1 |

### Отложеният случай — VideoOutputNode

`VideoOutputNode.cpp:947` прави `new VideoGLBlitWidget()`. Миграцията му е
**задължителна**, но е отложена за след обединението с веригата
(REQ-SW-PL-053 video-display-unification), защото веригата пренарежда точно този
файл и сменя начина на доставяне на widget-а: в `VideoOutputNode.cpp:224` дисплеят
се слага в `QSplitter` на жива страница и се добавя през
`m_display->widget()` (интерфейс `VideoDisplayWidget`), а не през
`embeddedWidget()`. Provider, построен върху `embeddedWidget()`, в тази схема
никога не се извиква.

Решение на потребителя (2026-09-29): живата страница с видеото е крайна цел, не
междинно решение. Затова миграцията на `VideoOutputNode` следва да е съгласувана
с начина, по който видеото се доставя след обединението — не автоматично по
модела на останалите 12.

## Бележка

Изискването е създадено по решение на потребителя (2026-09-07) след
проучването на SDRangel (Core/GUI split: същият PluginInterface, GUI factory-тата
връщат nullptr headless; два бинарника sdrangel/sdrangelsrv; Message/MessageQueue
за между-нишкова комуникация). Архитектурното предложение е в
`docs/Architecture/runtime-mode-architecture.md` (секция 4, Фаза 2). Unit тестове
са ОТЛОЖЕНИ по стоящата инструкция „НОВИ ТЕСТОВЕ СТОП".