# REQ-SW-PL-051 — Core/GUI Split: Изпълнителен план

- **Статус:** ~~ACTIVE~~ → **SUPERSEDED (2026-09-30)** — НЕ Е ИМПЛЕМЕНТИРАН
- **Бранч:** `feat/REQ-SW-PL-051-core-gui-separation` (base `develop_pre`)
- **Дата:** 2026-09-25
- **Одобрен дизайн:** `/tmp/opencode/core-gui-split-design.md` (v0.2)
- **QtNodes решение (потребител):** QtNodes остава в DaqsterCore; АС 2 се
  проверява **функционално** (headless процес без widget/сцена инстанции), не с
  `ldd | grep Qt5Widgets`. Документацията се обновява съответно.

> ## ⚠️ ТОЗИ ПЛАН НЕ Е ИЗПОЛНЕН — ЗАПИСАН Е ЗА ИСТОРИЯ
>
> Планът предвижда преструктуриране на **две библиотеки** `DaqsterCore` /
> `DaqsterGui`, пренос на `src/core/` → `src/frame_work/base/src/`, изтриване на
> `src/gui/`, отделно приложение `DaqsterHeadless`, `DAQSTER_BUILD_STATIC` +
> `generate_export_header(DaqsterCore)`, merge в `develop_pre` + Merge 3 (CP-5).
>
> **Нито едно от тези неща е извършено.** Реалното състояние:
>
> | Планирано тук | Реално |
> |---|---|
> | Библиотеки `DaqsterCore` / `DaqsterGui` | Plugin-и `FrameworkCore` / `FrameworkGui` (REQ-SW-PL-053) — има си собствен CMake език |
> | `src/core/` → `src/frame_work/base/src/` | `src/core/` и `src/gui/` отдавна го няма; миграцията е безпредметна |
> | Отделно приложение `DaqsterHeadless` | `NodeRunner --headless --run` (един dual-mode бинарник, REQ-SW-PL-053) |
> | `generate_export_header(DaqsterCore)` | `generate_export_header(FrameworkCore)` / `(FrameworkGui)` |
> | Merge в `develop_pre` → Merge 3 → `develop` | Branch-ът е върху `develop`; Merge 3 не е извършван |
> | HeadlessEngine без `DataFlowGraphicsScene` | Не е извършено; изисква REQ-SW-PL-052 (DONE) отделен pass |
>
> **Какво наистина беше направено** (виж реалния план и резултата):
> `docs/Architecture/core-gui-split.md` и
> `DevelopmentProcess/requirements/active/plugins/REQ-SW-PL-051-core-gui-separation-headless.md`.
> Ключовото решение е capability интерфейсът **`IWidgetProvider`**
> (`src/plugins/common/capabilities/IWidgetProvider.h`) вместо пренареждане на
> библиотеки — plugin-ите вече бяха разделени от REQ-SW-PL-053 и прилагат
> widget-и през `ChatGraphModel::nodeData(NodeRole::Widget)`, което е единственото
> място в дървото, където се вика `provider->createWidget()`.
>
> **Останък:** 2 от 30 модела не са разделени — `VideoOutput` (чака
> REQ-SW-PL-053 video-display-unification) и `AudioDisplayObsolete`.

## Цел

Финализиране на core/gui сплита до билдваемо състояние: framework/plugins/apps
като единствени разделения; `DaqsterCore`/`DaqsterGui` — две библиотеки от
`frame_work/`; `src/core/` и `src/gui/` изтрити; един `DAQSTER_BUILD_STATIC` +
`generate_export_header()`; merge в `develop_pre` + Merge 3 (CP-5) + консолидация;
актуализация на архитектурната документация.

## Целева структура (дизайн §3)

```
src/frame_work/
├── base/src/                      # → DaqsterCore (QtCore + QtNodes)
│   ├── include/                   # plugin контракт headers
│   ├── discovery/ persistence/ registry/ platform/ process/ perf/ logging/
│   └── engine/                    # HeadlessEngine, FlowLoader
├── base/src/gui/                  # → DaqsterGui (QtWidgets)
│   └── QPluginManagerGui, DebugConsoleWidget, PluginDetails, QPluginListView (+ .ui)
└── CMakeLists.txt                 # create_internal_library(DaqsterCore) + create_internal_library(DaqsterGui)
```

## Стъпки

### 1. Файлова миграция (git mv)
- `src/core/plugin/*.cpp` → `src/frame_work/base/src/` (заменя старите)
- `src/core/plugin/include/*.h` → `src/frame_work/base/src/include/`
- `src/core/plugin/{discovery,registry,persistence}/*` → `src/frame_work/base/src/{...}/`
- `src/core/platform/logging/process/perf/*` → `src/frame_work/base/src/{...}/` (logging ново)
- `src/core/engine/*` → `src/frame_work/base/src/engine/` (ново)
- `src/gui/plugin/*` → `src/frame_work/base/src/gui/` (заменя старите; +fix-ите от uncommitted)
- Изтриване: `src/core/CMakeLists.txt`, `src/gui/CMakeLists.txt`, `src/gui/{editor,builtin_nodes,library}/`, `src/core/{capabilities,data}/`
- IStoppable/IStartable → канонично `plugins/common/capabilities/` (INodeProvider вече там)

### 2. Engine почистване (QtCore + QtNodes, без scene)
- HeadlessEngine: маха `m_scene` (DataFlowGraphicsScene) — buildGraphModel()
  създава нов модел при всеки load; `clearScene()` не е нужен (нов m_graphModel)
- FlowLoader: маха `tempScene.clearScene()` (caller-ът пресъздава graphModel)
- Пренасочване на include-ите (IStoppable/IStartable/INodeProvider от
  plugins/common; NodeData-тата са в plugins/common/NodeDataTypes)

### 3. CMake — frame_work
- `frame_work/CMakeLists.txt`: `create_internal_library(DaqsterCore…)` (без gui/)
  + `create_internal_library(DaqsterGui…)` (gui/, зависи от DaqsterCore)
- `ComponentTemplates.cmake`: адаптация на `create_internal_library` за
  STATIC (`DAQSTER_BUILD_STATIC`) + `INCLUDE_DIRECTORIES`/`COMPILE_DEFINITIONS`
- Root `CMakeLists.txt`: `add_subdirectory(src/frame_work)` вместо
  `src/core`+`src/gui`; `option(DAQSTER_BUILD_STATIC … OFF)`

### 4. build_cfg — ЕДИН конфиг (дизайн §4)
- `generate_export_header(DaqsterCore BASE_NAME daqster_core)` → `daqster_core_export.h` (DAQSTER_CORE_EXPORT)
- `generate_export_header(DaqsterGui BASE_NAME daqster_gui)` → `daqster_gui_export.h` (DAQSTER_GUI_EXPORT)
- Всички `#include "build_cfg.h"` в core/gui headers → генерираните export headers
- frame_work/base/src/include/build_cfg.h остава като единствен legacy (по желание)

### 5. Плъгини + apps
- node_editor_ide: include dirs (94-95/189-190) → frame_work base/src (+/gui);
  RuntimeShell/NodeEditorIdeObject — теlkt към node_editor_ide версиите
- demo_nodeditor_nodes_core: REQUIRES DaqsterCore; include `src/core|src/gui/library` →
  `plugins/common` (+GL/TexturePool от plugins/common/GL); GUI-Nodes конвенция
- demo_nodeditor_nodes_gui: REQUIRES DaqsterGui; include → frame_work base/src/gui
- requirements_manager: include dirs → frame_work base/src (+/gui)
- apps/Daqster → DaqsterGui; apps/DaqsterHeadless → DaqsterCore (QtCore-only)

### 6. Bилд и проверки
- Build Qt6 → Qt5 → `./scripts/check-perf-degradation.sh`
- AC проверки: `--run test.flow` (headless), 0 widget инстанции, GUI factory nullptr
- ctest (DAQSTER_BUILD_TESTS) ако е относимо

### 7. Документация
- `core-gui-split.md`, `BuildSystemArchitecture.md`, `runtime-mode-architecture.md`
  (ред 76 „ALL NodeData types" → вариант 1; ldd-grep АС → функционална),
  `framework/README.md`, `plugins/README.md`, `PluginDevelopment.md`

### 8. Git
- Commit → merge в `develop_pre` → Merge 3 (CP-5, `2b2ad9f`) → `develop` + push
- Changelog актуализация; close REQ-SW-PL-051

## Рискове
- demo_nodeditor_nodes_core съдържа widget класове (ChatBaseWidget, AudioSourceDataModelUI)
  — действа се според „headless = без canvas, НЕ без widgets" (потребител)
- Core плъгин моделите са QtCore-compatible; _gui осигурява UI през NodeWidgetFactory