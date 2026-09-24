# Daqster Flow File Format (`.flow`)

> **Статус:** справка, създадена след Merge 1 (CP-1+2+3) в `develop_pre`.
> Описва реалния формат, който кодът чете/пише (`FlowUiSection.{h,cpp}`), а не предложение.
> архитектурната визия е в `../runtime-mode-architecture.md`.

## 1. Обща структура

`.flow` е **JSON документ** със следните коренови секции:

| Ключ | Тип | Задължителен? | Описание |
|---|---|---|---|
| `nodes` | array | ✅ | Списък на нодовете (QtNodes формат: `id`, `internal-data.model-name`, `label`, `labelVisible`, `position{x,y}`, per-node данни) |
| `connections` | array | ✅ | Връзки: `{outNodeId, outPortIndex, inNodeId, inPortIndex}` |
| `groups` | array | ⬜ | QtNodes групи (по избор) |
| `ui` | object | ⬜ | **Runtime UI състояние** (въведено от REQ-SW-PL-049) — само за scenes, които са били показвани в runtime/реди-сесия |

`ui` секцията е **опционална** → файлове без нея остават backward compatible и се зареждат нормално.

## 2. `ui` секция (REQ-SW-PL-049, FlowUi::UiSection)

```json
"ui": {
  "version": 1,
  "workspaces": [
    {
      "id": 0,
      "tabbed": true,
      "geometry": { "x": 100, "y": 100, "w": 800, "h": 600, "maximized": false }
    }
  ],
  "nodes": {
    "0": {
      "deembedded": true,
      "workspace": 0,
      "geometry": { "x": 150, "y": 150, "w": 300, "h": 200, "maximized": false },
      "autoStart": true
    },
    "1": {
      "deembedded": false,
      "workspace": 0,
      "autoStart": false
    }
  }
}
```

### 2.1 Поле по поле

**`version`** (int, default `1`)
- Семантично версиониране на `ui` секцията.
- Парсерът приема само `1`; `version > 1` → секцията се **игнорира** (qWarning), файлът пак се зарежда без UI състояние.

**`workspaces`** (array of `WorkspaceUi`)
| Поле | Тип | Описание |
|---|---|---|
| `id` | int | MDI workspace идентификатор |
| `tabbed` | bool | Дали workspace-ът е tabbed (за разлика от каскадни/свободни sub-windows) |
| `geometry` | object | Виж Geometry по-долу |

**`nodes`** (object, ключове = `QtNodes::NodeId` като низ, напр. `"0"`)
- Стойност = `NodeUi` **само за нодове с embedded widget** (т.е. нодове, които показват UI).
- Не-числови ключове се пропускат при зареждане.

**`NodeUi`** полета:
| Поле | Тип | Описание |
|---|---|---|
| `deembedded` | bool | Дали widget-ът на нода е отделен (deembedded) от canvas-а |
| `workspace` | int | В кой workspace е поставен нодът |
| `geometry` | object | Присъства **само когато `deembedded: true`** — позиция/размер на отделения widget |
| `autoStart` | bool | Дали нодът да се стартира автоматично при `--run` (виж §3) |

**`Geometry`** — custom формат `{x, y, w, h, maximized}` (NOT `QWidget::saveGeometry` — ненадежден за MDI sub-windows; урок от SDRangel):
| Поле | Тип | Описание |
|---|---|---|
| `x`, `y` | int | Позиция (екранни координати за деembed/detached widget) |
| `w`, `h` | int | Размер |
| `maximized` | bool | Maximize флаг |

### 2.2 Поведение при зареждане

- `FlowUi::UiSection::fromJson()`: не-числови node ключове → skip; `version > 1` → целият `ui` блок се игнорира.
- При липсващ `geometry` за деembed-нат нод — използват се стойности по подразбиране (0,0,0x0).

## 3. `autoStart` семантика

- **Файлово-ниво поле** в `ui.nodes[<id>].autoStart` (default `false`).
- При `--run <flow>` RuntimeShell-ът чете флага и вика `IStartable::start()` върху нодовете, които го имплементират (source/sink нодове с „start" действие). Виж `shared/IStartable.h`.
- **Днес НЕ е конфигурируемо от IDE.** Няма UI контрол (checkbox/context menu), който да го записва. Единствените начини да се зададе:
  - ръчно редактиране на `.flow` JSON (perf/harness flows — `tests/data/*.flow`),
  - програмен път чрез dev driver env vars (`DAQSTER_AUTOSTART_VIDEO`, `DAQSTER_AUTOSTART_FLOW`).
- Нишов дизайнерски избор (A3): при `autoStart: false` flow-ът стартира „студен" и потребителят пуска всеки нод ръчно от неговите (docked/detached) контроли.

## 4. Runtime mode `--run` (REQ-SW-PL-048)

- CLI: `Daqster --run <path/to/scene.flow>` — зарежда scene и прилага `ui` секцията (workspaces, deembed state, geometry, autoStart) в runtime shell вместо редактор.
- MDI workspaces: `RuntimeShell` управлява sub-windows; `tabbed`/cascade според `workspaces[]`.
- Env vars за автоматизация (dev/perf drivers):
  - `DAQSTER_AUTOSTART_VIDEO=1` — програмен video-source flow (dev driver)
  - `DAQSTER_AUTOSTART_FLOW=<path>` — зарежда произволен `.flow` headlessly
- Deadlines/приоритет на `autoStart` изпълнение: при `--run` RuntimeShell минава по `ui.nodes` и за всеки нод с `autoStart:true` → `IStartable::start()`.

## 5. Интерфейси (CP-1/CP-3)

### `IStoppable` (`src/plugins/demo_nodeditor_nodes/shared/IStoppable.h`)
```cpp
namespace Daqster {
class IStoppable {
    virtual ~IStoppable() = default;
    virtual void stop() = 0;
};
}
```
- Интерфейс за node модели с background работа (threads/timers/processes) — **само** нодове, които искат явно спиране.
- **Idempotent:** `stop()` трябва да е безопасно за многократно извикване.
- Реализация: всички source/sink нодове с QThread/QTimer/QProcess (по списъка в runtime-mode-architecture.md §4.3).

### `IStartable` (`src/plugins/demo_nodeditor_nodes/shared/IStartable.h`)
```cpp
namespace Daqster {
class IStartable {
    virtual ~IStartable() = default;
    virtual void start() = 0;
};
}
```
- Интерфейс за node модели, стартиращи се **програмно** (runtime autoStart, REQ-SW-PL-048).
- Отделен от IStoppable: display/processing нодове остават не-стартируеми.
- Реализация: само source/sink нодове с user-facing „start" действие.

## 6. Примерни файлове

- `tests/data/autostart_test.flow`, `tests/data/runtime_test.flow`, `tests/data/system_monitor_test.flow` — всички с `ui` секция (примерен формат за тестове).
- `tests/data/video_1view.flow` и др. — perf scenarios (с `ui` секции, добавени при CP-5).