# Тестова стратегия — Daqster (public repo)

**Версия на документа:** 1.0
**Дата:** 2026-09-02
**Обхват:** NodeEditor scene save/load регресионно тестване + video perf тестване + verification gate

> Този документ описва как тестваме Daqster (публичното repo) — фокус върху
> регресионното тестване на NodeEditor scene save/load (REQ-SW-PL-037) и
> препратките към съществуващото video perf тестване. Документът живее в
> `DevelopmentProcess/` (а не в `.opencode/`), защото `.opencode/` е gitignored
> и не е част от repo-то.

---

## 1. Scene save/load регресионно тестване (REQ-SW-PL-037)

### 1.1 Тестови сценарии — `tests/data/*.flow`

Файловете `.flow` в `tests/data/` са JSON сцени във формата на
`QtNodes::DataFlowGraphModel::save()`:

- `nodes[]` — всеки нод има `id`, `internal-data` (с `model-name` + параметрите
  от `save()` на модела), `label`, `labelVisible`, `position{x,y}`;
- `connections[]` — всяка връзка има `outNodeId`, `outPortIndex`, `inNodeId`,
  `inPortIndex`.

| Файл | Какво покрива | Нодове | Връзки |
|------|---------------|--------|--------|
| `video_graph.flow` | Основен video pipeline: source → effect → output | 3 | 2 |
| `multi_effect_video.flow` | Два ефекта в серия (GPU-resident chain): source → effect → effect → output; параметри на двата ефекта (`effect` id + brightness/contrast) | 4 | 3 |
| `number_graph.flow` | Built-in numeric нодове: `NumberSource` → `NumberResult`; параметри (`type`, `randomEnabled`, `interval`, `number`) | 2 | 1 |
| `audio_graph.flow` | Аудио pipeline: `AudioSource` → `AudioDisplay` (alias key → SampledData display) | 2 | 1 |
| `display_aliases.flow` | Display consolidation: `AudioSource` → 3 display-а (`DaqDisplay`, `GenericDisplay`, `AudioDisplay`) — и трите разрешават към DaqDisplayNode-базирани модели | 4 | 3 |
| `llm_graph.flow` | LLM pipeline: `LLamaModel` → `Console`; параметри (`host`, `port`, `ctxSize`, `useGpu`) | 2 | 1 |
| `empty_scene.flow` | Празна сцена (`nodes: []`, `connections: []`) — load не трябва да crash-ва и да дава празна сцена | 0 | 0 |
| `missing_node.flow` | Нерегистриран нод тип (`BogusUnregisteredNode`) — толерантно зареждане: warn + skip, без crash | 3 (1 невалиден) | 3 (2 остават) |
| `malformed.flow` | Невалиден/скъсан JSON (липсващи closing скоби) — load не трябва да crash-ва | — | — |

**Забележка за имената на нодовете:** имената в `internal-data.model-name`
трябва да съвпадат с реално регистрираните имена (връщани от `name()` на
модела), а не с имената на класовете:

- `NumberSourceDataModel::name()` → `"NumberSource"`;
- `NumberDisplayDataModel::name()` → `"NumberResult"` (НЕ `"NumberDisplay"`);
- `AudioSourceDataModel::name()` → `"AudioSource"`;
- `AudioDisplayAlias::name()` → `"AudioDisplay"` (alias на `DaqDisplayNode` — SampledData display);
- `LLamaModelDataModel::name()` → `"LLamaModel"`;
- `ConsoleDataModel::name()` → `"Console"`;
- `VideoFileSourceNode::name()` → `"VideoFileSource"`;
- `VideoEffectNode::name()` → `"VideoEffect"`;
- `VideoOutputNode::name()` → `"VideoOutput"`.

Регистрацията е в `src/plugins/node_editor_ide/NodeEditorIdeObject.cpp:145-153`
(built-in) и `src/plugins/demo_nodeditor_nodes/DemoNodeEditorNodesObject.cpp:88-128`
(demo нодове).

### 1.2 Как се използват

1. **Ръчно (NodeEditor IDE):** стартирай `build_qtX/bin/Daqster` → File →
   "Load Scene…" (Ctrl+O) → избери `.flow` файла. Провери:
   - нодовете се появяват с коректни позиции и label-и;
   - връзките са възстановени (брой = очакваният);
   - параметрите на нодовете са възстановени (напр. `VideoEffect` показва
     избрания ефект и стойностите на слайдерите).
2. **Програмно (loadSceneTolerant):** `NodeEditorIdeObject::loadSceneTolerant()`
   (`src/plugins/node_editor_ide/NodeEditorIdeObject.cpp:225-304`) чете файла,
   маха нерегистрираните нодове и dangling връзките, после вика
   `graphModel()->load()`. Същият път се ползва и от File менюто.
3. **Round-trip проверка:** след load → Save Scene… → сравни новия файл с
   оригиналния (node count, connection count, параметри). Разлики в параметрите
   = регресия.

### 1.3 Очаквано поведение при edge case-и

- **`missing_node.flow`:** `BogusUnregisteredNode` (id 1) се пропуска;
  връзките `0→1` и `1→2` се махат (dangling); остават нодове 0 и 2 + връзката
  `0→2`. Потребителят получава warning диалог/лог с името на пропуснатия тип.
  **НЕ трябва да crash-ва.**
- **`malformed.flow`:** `QJsonDocument::fromJson()` връща parse error →
  `loadSceneTolerant` логва warning и връща `false`. **НЕ трябва да crash-ва.**
- **`empty_scene.flow`:** зарежда празна сцена без грешки.

### 1.4 Как се открива регресия

Сравни зареденото състояние на сцената с очакваното:

| Сценарий | Очакван node count | Очакван connection count | Ключови параметри |
|----------|--------------------|--------------------------|-------------------|
| `video_graph.flow` | 3 | 2 | — |
| `multi_effect_video.flow` | 4 | 3 | effect[0]=`brightness` (15), effect[1]=`contrast` (140) |
| `number_graph.flow` | 2 | 1 | `type=double`, `number="42"` |
| `audio_graph.flow` | 2 | 1 | — |
| `llm_graph.flow` | 2 | 1 | `host=127.0.0.1`, `port=8080`, `ctxSize=2048` |
| `empty_scene.flow` | 0 | 0 | — |
| `missing_node.flow` | 2 (след skip) | 1 (след clean-up) | — |
| `malformed.flow` | 0 (load отказан) | 0 | — |

Автоматизацията (бъдеща): unit тест, който зарежда всеки `.flow` през
`DataFlowGraphModel::load()` и проверява node/connection count + параметри;
`malformed.flow` се проверява, че load връща грешка без crash.

---

## 2. Video perf тестване (референция)

- **Perf снапшот:** `tests/performance/performance-video-display-2026-08-13.md` —
  пълният snapshot на video display перформанс изследването (методология,
  числови резултати, perf анализ, промени). Служи за репликация на измерванията.
- **Skill:** `daqster-perf-testing` (частното repo, `.opencode/skills/`) —
  Qt5/Qt6 perf harness, GL blit измервания, perf профилиране.
- **Програмен graph builder:** `NodeEditorIdeObject::autoStartVideo()`
  (`src/plugins/node_editor_ide/NodeEditorIdeObject.cpp:311-474`) строи
  `VideoFileSource → VideoOutput` граф програмно и стартира playback без GUI
  интеракция. Env vars:
  - `DAQSTER_AUTOSTART_VIDEO=1` — включва autoStartVideo;
  - `DAQSTER_VIDEO_FILE=<файл>` — файлов source (приоритетен);
  - `DAQSTER_STREAM_URL=<url>` — stream source (fallback);
  - `DAQSTER_AUTOSTART_EFFECT=<effectId>` — вмъква `VideoEffect` между source и output;
  - `DAQSTER_AUTOSTART_EFFECT2=<effectId>` — вмъква втори ефект (GPU-resident chain);
  - `DAQSTER_AUTOSTART_SAMPLER=1` — вмъква `FrameSampler`;
  - `DAQSTER_SCENE_VIDEO=1` — in-scene video (Qt6, REQ-SW-PL-021);
  - `DAQSTER_GL_BLIT=1` — GL blit път (вж. perf документа).
  - `DAQSTER_AUTOSTART_FLOW=<path>` — headless зареждане на `.flow` сцена при
    старт (REQ-SW-PL-038); ако `DAQSTER_VIDEO_FILE` е зададен, върху заредената
    сцена се изпълнява `startVideoPlayback()` (configure source + Play + Perf).

---

## 3. Verification gate (RDD-PROCESS.md)

Според `DevelopmentProcess/requirements/RDD-PROCESS.md` (частното repo),
всяко изискване минава през следния гейт преди статус DONE:

1. **Имплементация** — код, отговарящ на Acceptance Criteria;
2. **Qt5 build** — успешна компилация;
3. **Qt6 build** — успешна компилация;
4. **Unit тестове** — `tests/` в публичното repo, `-DDAQSTER_BUILD_TESTS=ON`
   (28 теста);
5. **Headless smoke test** — приложението/plugin-а се стартира и работи без GUI;
6. **Статусът на изискването се обновява** — AC checkboxes + status field.

За REQ-SW-PL-037: Qt5 + Qt6 builds PASS, ръчен smoke (save → clear → load) —
вж. `DevelopmentProcess/requirements/active/plugins/REQ-SW-PL-037-node-editor-scene-save-load.md`.

---

## 4. Поддръжка на този документ

- При добавяне на нов `.flow` сценарий в `tests/data/` — обнови таблиците в
  секция 1.1 и 1.4.
- При промяна на регистрираните имена на нодове — провери дали `.flow`
  файловете все още зареждат (missing-node пътят ги skip-ва, но сценарият
  губи смисъл).
- При промяна на `autoStartVideo()` env vars — обнови секция 2.