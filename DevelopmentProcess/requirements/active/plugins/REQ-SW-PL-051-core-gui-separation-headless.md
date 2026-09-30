# REQ-SW-PL-051: Core/GUI разделение + headless изпълнение на flow

- **Статус:** ACTIVE — миграцията на node моделите е завършена с 1 изключение
  (вж. „Оставаща работа“); AC 2 е блокиран, не е изпълнен
- **Приоритет:** Medium
- **Отговорник (роля):** Ivan (Implementation)
- **Дата:** 2026-09-07
- **Родител:** REQ-SW-PL-014
- **Зависи от:** REQ-SW-PL-048, REQ-SW-PL-050

## Описание

SDRangel-ски модел: всеки нод се разделя на core (логика, без GUI) и gui
(widget). Възможност за стартиране на flow headless (без Qt Widgets/OpenGL) —
`NodeRunner --headless --run <flow>`. Един source tree, два режима (като
sdrangel / sdrangelsrv).

## Acceptance Criteria

- [x] 1. Архитектурен шаблон core/gui за нодовете — `docs/Architecture/core-gui-split.md`
- [ ] 2. Headless стартиране на flow без Qt Widgets/OpenGL — **БЛОКИРАН**, виж „Блокер“
- [x] 3. GUI factory методите връщат nullptr в headless режим — връщат nullptr
       **винаги** (core моделите нямат widget изобщо); widget-ът идва от
       `IWidgetProvider` само в GUI режим
- [x] 4. Същият flow работи с и без GUI — `NodeRunner --run` (GUI) и
       `NodeRunner --headless --run` върху едни и същи `.flow` файлове
- [ ] 5. (Опционално) REST API за remote control — отложен; отделен
       REQ-SW-PL-054. Документиран в `docs/Architecture/rest-api-remote-control.md`
- [x] 6. Документация на шаблона — `docs/Architecture/core-gui-split.md`
- [ ] 7. Тестове — отложени по стоящата инструкция „НОВИ ТЕСТОВЕ СТОП“.
       Съществуващите `ctest` 11/11 са зелени на Qt5 и Qt6 (регресия, не покритие
       на split-а)

## Проследимост

- **Комити:**
  - `2705a3e` (REQ-SW-PL-052, prerequisite)
  - `878d95d` — премахване на 32 дублирани widget файла
  - `5860d65` — завършване на миграцията + 5-те runtime bug-а, които тя откри
  - `99d24ba` — документиране на 13-те останали миграции
  - *(pending)* — поправка на 2-те регресии, които `5860d65` внася в headless
    пътя (виж „Регресии, открити при прегледа“)
- **Код:**
  - `src/plugins/common/capabilities/IWidgetProvider.h` — capability интерфейсът
  - `src/plugins/demo_nodeditor_nodes_core/` — widget-free моделите
  - `src/plugins/demo_nodeditor_nodes_gui/NodeWidgetFactory.cpp` — `modelName → widget`
  - `src/plugins/node_editor_ide/BuiltInNodes/Library/types/ChatGraphModel.cpp` —
    кеширане на widget-а (`nodeData(NodeRole::Widget)`)
  - `src/frame_work/base/src/registry/PluginRegistry.cpp` — `capabilityInstances()`,
    `ensureInitialized()`
  - `docs/Architecture/core-gui-split.md`

## Състояние на миграцията (измерено в кода на 2026-09-30)

От 33-те регистрирани типа в `DemoNodeEditorNodesCoreObject::registerNodes()`:

| Състояние | Брой | Кои |
|---|---|---|
| Разделени — `embeddedWidget()` връща `nullptr`, widget-ът е в `_gui` | **28** | всички освен 2-те по-долу |
| **Неразделени** | **2** | `VideoOutputNode`, `AudioDisplayModelObsolete` |

28-те разделени типа имат и creator в `NodeWidgetFactory`, с едно изключение:
`DemuxNodeObsolete` / `MuxNodeObsolete` (+ техните alias-и `DemuxNode` /
`MuxNode`) връщат `nullptr`, но нямат creator. Това е **умишлено** — те са
obsolete routing модели без UI, рисувани като иконка в node-а.

### Неразделен #1 — VideoOutputNode (отложен по съществуващо решение)

`VideoOutputNode::embeddedWidget()` връща `m_widget`, а `m_widget` съдържа
`VideoGLBlitWidget` + `VideoPerfBadge`. Миграцията е **задължителна**, но е
отложена за след обединението с веригата (REQ-SW-PL-053 video-display-unification),
защото веригата пренарежда точно този файл и сменя начина на доставяне на
widget-а: дисплеят се слага в `QSplitter` на жива страница и се добавя през
`m_display->widget()` (интерфейс `VideoDisplayWidget`), а не през
`embeddedWidget()`. Provider, построен върху `embeddedWidget()`, в тази схема
никога не се извиква.

Решение на потребителя (2026-09-29): живата страница с видеото е крайна цел, не
междинно решение. Затова миграцията на `VideoOutputNode` следва да е съгласувана
с начина, по който видеото се доставя след обединението — не автоматично по
модела на останалите.

### Неразделен #2 — AudioDisplayModelObsolete (документирано тук, не е обсъждано)

`AudioDisplayModelObsolete` не наследява `QtNodes::NodeDelegateModel` направо, а
`QDevIoDisplayModelObsolete` (в `node_editor_ide/BuiltInNodes/Library/display/`),
чиито `embeddedWidget()` връзва `m_stack` в конструктора. Следствие: `_core`
плъгинът компилира **7 QtWidgets класа** от `BuiltInNodes/Library` (вж. подолу) и
`AudioDisplayObsolete` няма creator в фабриката.

Това е остатък от същата миграция, но с по-нисък приоритет от `VideoOutputNode`:
моделът е obsolete, не ползва в момента (`AudioDisplayAlias` → `DaqDisplayNode`
е активният път). Мигрирането изисква изнасяне на `QDevIoDisplayModelObsolete` и
`XYSeriesIODeviceObsolete` в `_gui` — т.е. същия модел като при VideoOutput, но
без блокиращата зависимост от REQ-SW-PL-053.

## Регресии, открити при прегледа (2026-09-30)

`5860d65` въведе `PluginRegistry::capabilityInstances()` + `ensureInitialized()` и
сложи `createPluginObject(hash, this)`. И двете се оказаха погрешни и бяха
поправени в следващия комит:

1. **`ensureInitialized()` в `nodeProviders()` / `instances()` товареше GUI
   plugin-а в headless процес.** `FrameworkGuiPluginObject` конструира
   `RuntimeShell` (MDI + QtWidgets + OpenGL) в конструктора си, така че
   Initialize-ването му при всяко probe за node providers издърпваше целия GUI
   стек в `--headless`. `registerNodes()` е `const` и работи на гол обект, затова
   `nodeProviders()` вече не инициализира; `instances(iid)` изпълнява
   `qt_metacast(iid)` **преди** `ensureInitialized()`, не след.

2. **Double-free при shutdown.** `createPluginObject(hash, this)` правеше plugin
   обектите QObject деца на registry-то, а `QPluginManager::ShutdownPluginManager()`
   вече ги `delete`-ва изрично през `allPluginInstances()` — `~QObject` ги
   delete-ваше втори път. Проявяваше се чак при teardown-а като heap corruption
   (`malloc_consolidate(): unaligned fastbin chunk detected`, exit 134), и само по
   пътищата, които `return`-ват от `main()` без да достигнат shutdown call-а.
   Обектите са parentless отново (`createPluginObject(hash, nullptr)`).

Проверено след поправката: `NodeRunner --headless --run` върху flow-и с
неподдържани headless типове излиза с **exit 1** (както на parent-а `99d24ba`),
вместо 134/139; `FrameworkGuiPlugin` не се появява в headless лога; Qt5/Qt6 builds
зелени; `ctest` 11/11 на двата; GUI runtime и IDE стартират чисто на 8 flow-а.

## Блокер за AC 2 (headless без QtWidgets)

AC 2 не е изпълнен и не може да бъде с текущия код. `NodeRunner` вече не
линква нито един Daqster plugin (само `FrameworkCore`), но:

1. `QApplication` (Qt5: в `QtWidgets`) остава необходим за headless — за
   `QTimer`, `QCoreApplication::exec()` и QtNodes сигналната инфраструктура.
2. `demo_nodeditor_nodes_core` още компилира QtWidgets код заради
   `AudioDisplayModelObsolete` (вж. по-горе) — 7 translation units от
   `BuiltInNodes/Library/{display,connectors,threading,decoders}` са вписани
   директно в `demo_nodeditor_nodes_core/CMakeLists.txt:274-286`.

Изисква: мигриране на `AudioDisplayModelObsolete` (REQ-SW-PL-051) и
промяна на `QApplication` → `QCoreApplication` в headless пътя на `NodeRunner`.
Второто е отделна промяна, не обхваната от този REQ.

## Бележка

Изискването е създадено по решение на потребителя (2026-09-07) след
проучването на SDRangel (Core/GUI split: същият PluginInterface, GUI factory-тата
връщат nullptr headless; два бинарника sdrangel/sdrangelsrv; Message/MessageQueue
за между-нишкова комуникация). Архитектурното предложение е в
`docs/Architecture/runtime-mode-architecture.md` (секция 4, Фаза 2). Unit тестове
са ОТЛОЖЕНИ по стоящата инструкция „НОВИ ТЕСТОВЕ СТОП“.
