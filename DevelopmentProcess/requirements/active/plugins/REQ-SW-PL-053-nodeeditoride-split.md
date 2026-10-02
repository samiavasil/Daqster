# REQ-SW-PL-053: Core/Gui разделяне на NodeEditorIde (plugin-based runtime)

- **Статус:** ACTIVE
- **Приоритет:** High
- **Отговорник (роля):** Ivan (Implementation)
- **Дата:** 2026-09-28
- **Родител:** REQ-SW-PL-051 (core/gui separation)
- **Зависи от:** REQ-SW-PL-051, REQ-SW-PL-052

## Описание

`NodeEditorIde` съдържа едновременно **редактор** (canvas, вграждане на widgets) и
**GUI runtime** (`RuntimeShell` — MDI работни пространства с deembedded widgets).
Затова `NodeRunner` го линкваше, а с него и `FrameworkGui` → `QtWidgets`, дори в
headless mode.

Решението е `NodeRunner` да **не знае нищо за конкретен plugin**. Той линква само
`FrameworkCore` и на startup пита `QPluginManager` кой зареден plugin може да
изпълни `.flow` в желания мод:

- `--headless` → `FrameworkCorePlugin` (`HeadlessEngine`)
- по подразбиране → `FrameworkGuiPlugin` (`RuntimeShell`)

Това изисква capability интерфейс, през който runner-ът вижда engine-а абстрактно
(без `#include` на конкретен plugin header и без link-time зависимост) —
`Daqster::IRuntimeHost`, открит през `QPluginManager::runtimeHosts(RuntimeMode)`.

## Ограничение (установено при реализацията)

Изискването „headless binary **без** QtWidgets" се оказа **непостижимо** в
текущата архитектура, и то по две независими причини:

1. **`QApplication` е в `QtWidgets` при Qt5.** Dual-mode binary, който може да
   показва GUI, трябва да линква `QtWidgets` и при `--headless`. В Qt6
   `QApplication` е в `QtGui`, но `QWidget` остава в `QtWidgets`.
2. **Node моделите конструират widgets в конструктора си.** Дори изграждане с
   `QGuiApplication` (без Widgets на link line-а) пада. Доказан backtrace:

   ```
   #8  QWidgetPrivate::init()                 libQt5Widgets.so.5
   #9  NumberSourceDataUi::NumberSourceDataUi(QWidget*)
   #10 NumberSourceDataModel::NumberSourceDataModel()
   ```

   `configureHeadlessPlatform()` документира същото ограничение — headless
   изисква `QApplication`, не `QCoreApplication`.

Премахването на `QtWidgets` изисква **per-node core/GUI split** — node моделите
да не строят widgets в ctor-а си, а да ги създават lazily през `createWidget()`.
Това е останалата част от REQ-SW-PL-051.

Постигнатото е по-силната архитектурна гаранция: **`NodeRunner` не линква нито
един Daqster plugin** — само `FrameworkCore`. Целият избор на engine става runtime
решение.

## Acceptance Criteria

- [x] 1. `NodeEditorIde` разделен на `FrameworkCorePlugin` + `FrameworkGuiPlugin`;
      `RuntimeShell` премесен от `node_editor_ide` в `FrameworkGuiPlugin`,
      мъртвият `NodeEditorIdeObject::RunRuntime()` премахнат
- [x] 2. `FrameworkCorePlugin` съдържа `HeadlessEngine` и `registerNodesHeadless()`
- [x] 3. `FrameworkGuiPlugin` съдържа `RuntimeShell`, video display widgets и
      `registerNodes()`; `NodeEditorWidget`/`CustomDataFlowScene`/`FlowUiSection`
      преместени в `NodeEditorLibrary`, защото `RuntimeShell` ги използва
- [x] 4. `IRuntimeHost` capability + `QPluginManager::runtimeHosts(RuntimeMode)`;
      `NodeRunner` зарежда plugin-а динамично според мода, без да реферира
      конкретен plugin тип
- [x] 5. Един dual-mode application (`NodeRunner`): `--headless` и GUI от една
      инсталация, plugin-ът се избира по опции
- [x] 6. `NodeRunner` линква **само** `FrameworkCore` от страната на Daqster
      (проверено с `readelf -d`: няма `NodeEditorIde`, `FrameworkGuiPlugin`,
      `FrameworkCorePlugin`, `DemoNodeEditorNodesCore`)
- [ ] 7. Headless binary **без** `QtWidgets` — **BLOCKED**, виж „Ограничение"
      по-горе. Изисква per-node core/GUI split (продължение на REQ-SW-PL-051)
- [x] 8. Размери: `NodeRunner` 56 KB (Qt5) / 90 KB (Qt6) — под 5 MB;
      `libFrameworkGuiPlugin.so` 653 KB — под 20 MB
- [x] 9. Тестовете минават — `ctest` 11/11 на Qt5 и Qt6; smoke run в двата мода
      и на двата Qt варианта

## Проследимост

- **Коммити:** `75cd6c4` (split + dual-mode runner), `9b13516` (FrameworkCorePlugin)
- **Код:** `src/plugins/common/capabilities/IRuntimeHost.h` (новият capability),
  `src/frame_work/base/src/registry/PluginRegistry.{h,cpp}` +
  `QPluginManager.{h,cpp}` (`runtimeHosts()`), `src/plugins/FrameworkCorePlugin/`,
  `src/plugins/FrameworkGuiPlugin/` (с `RuntimeShell.{h,cpp}`),
  `src/apps/NodeRunner/` (`main.cpp` + CMake)
- **Rollback tag:** `req-52-done`
