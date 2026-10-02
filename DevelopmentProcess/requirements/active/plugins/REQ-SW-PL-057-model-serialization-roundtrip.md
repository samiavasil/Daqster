# REQ-SW-PL-057 — Model Serialization Round-Trip (`model-name`)

> **Parent:** REQ-SW-PL-037 (scene save/load regression scenarios)
> **Depends on:** REQ-SW-PL-051 (core/gui separation) — bug surfaced during its manual verification
> **Scope:** `NodeDelegateModel::save()` contract in `demo_nodeditor_nodes_core`

## Проблем

`.flow` файлове, записани от редактора, не могат да бъдат заредени обратно за
10 от 33 типа нодове. При запис няма грешка и няма предупреждение — файлът
изглежда валиден. При зареждане нодът се отхвърля тихо:

```
[WRN] loadFlow: skipped unregistered node types: "<unnamed>"
```

и всяка връзка, която го докосва, се изтрива заедно с него. При файл с 2 нода
и 1 връзка резултатът е `nodes= 1 connections= 0`.

### Причина

Сериализаторът на графа определя типа на нода от
`<node>["internal-data"]["model-name"]` (`DataFlowGraphModel::loadNode`).
Този ключ се записва от **базовия** `NodeDelegateModel::save()`.

10 модела override-ват `save()` и създават **празен** `QJsonObject` вместо да
започнат от резултата на базата:

```cpp
QJsonObject modelJson;
modelJson["name"] = name();          // ← ключ, който никой не чете
```

Ключът `model-name` отпада, а `name` не се чете от нито един `load()` —
мъртви данни, които при това убиват идентичнота на нода. При зареждане името е
празно, `registry->create("")` връща `nullptr` и нодът се пропуска.

Засегнати: `AudioSource`, `AudioSourceObsolete`, `DemuxNodeObsolete`,
`MuxNodeObsolete`, `NumberResult`, `Arithmetic/Logic`, `Modulo`, `GpuMonitor`,
`JackDetect`, `PcapCapture`.

**Дефектът е стар**, не е регресия от core/gui split-а — същият код е на
`develop` от 17 септември 2026. Флоутази типове никога не са се зареждали обратно.

### Защо тестовете не го хванаха

`tests/data/*.flow` са **ръчно написани fixtures** (commit `7b2d9fe`,
2 септември 2026), а не продукт на round-trip. Ръчно са добавени и `model-name`,
и `name`, за да минават. Нито един тест не твърди, че изходът на `save()` може
да бъде зареден — сляпата точка, която е оставила дефекта жив.

## Изисквания

### AC1 — `save()` включва `model-name`
Всеки `NodeDelegateModel` override на `save()` започва от резултата на базата:

```cpp
QJsonObject modelJson = NodeDelegateModel::save();
```

Ключът `model-name` се записва от базата и не се презаписва от наследниците.

### AC2 — мъртвият ключ `name` е премахнат
Никой `load()` не чете `name`; наличието му до `model-name` е точно
причината за объркването и трябва да отпадне.

### AC3 — собствените ключове на модела се запазват
Началото от базовия обект добавя `model-name`, без да измества
`type` / `inputs` / `intervalSeconds` — ключовете, които `load()` чете.

### AC4 — регресионен тест
`demo_nodeditor_modelname_tests` инстанцира всеки засегнат модел и изисква
`save()` да носи `model-name == name()`. Тестът е доказан, че улавя дефекта:
счупеният `AudioSourceDataModel` го прави FAIL.

## Статус: DONE

- [x] 1. `AudioSourceDataModel::save()` — `NodeDelegateModel::save()` вместо празен обект
- [x] 2. `AudioSourceDataModelObsolete::save()` — същото
- [x] 3. `DemuxNodeObsolete::save()` — същото
- [x] 4. `MuxNodeObsolete::save()` — същото
- [x] 5. `NumberDisplayDataModel::save()` — същото
- [x] 6. `ArithmeticLogicModel::save()` — същото
- [x] 7. `ModuloModel::save()` — същото
- [x] 8. `GpuMonitorModel::save()` — същото
- [x] 9. `JackDetectModel::save()` — същото
- [x] 10. `PcapModel::save()` — същото
- [x] 11. `demo_nodeditor_modelname_tests` — 11 теста, хващат дефекта (доказано)
- [x] 12. Build Qt5 + Qt6 чист; `ctest` **12/12 Qt5 + 12/12 Qt6** (беше 11/11)
- [x] 13. A/B проверка на живо: оригиналният файл → `nodes= 1 connections= 0`
      + `<unnamed>`; същият файл с добавен `model-name` → `nodes= 2 connections= 1`,
      без предупреждение

## Известно ограничение

**Съществуващи файлове на диск остават нечетими.** Фиксът поправя `save()`, не
`load()`, а loader-ът е в submodule-а `src/plugins/external_libs/nodeeditor`
(отделен repo) и не може да се толерира тук. Файлове, записани с дефекта, ще
изискват повторно записване от редактора.

Следваща стъпка (separate REQ, приоритет по избор): толерантен loader в
nodeeditor, който приема `name` като fallback за `model-name` — това би
спасло вече записаните флоута без ръчна намеса.

## Проследимост

- **Код:** 10 файла в `src/plugins/demo_nodeditor_nodes_core/`
  (`{Displays/NumberDisplay, Operators/ArithmeticLogic, Operators, Routing/Demux,
  Routing/Mux, Sources/AudioSource ×2, Sources/GpuMonitor, Sources/JackDetect,
  Sources/Pcap}`)
- **Тест:** `tests/plugins/demo_nodeditor_nodes/test_model_name_serialization.{h,cpp}`
  + таргет `demo_nodeditor_modelname_tests`
- **Не е покрит от теста:** 3-те `*Obsolete` модела (`AudioSourceDataModelObsolete`,
  `DemuxNodeObsolete`, `MuxNodeObsolete`) — инстанцирането им влачи целия legacy
  QDevIO audio/display стек до `QDevioDisplayModelUiObsolete` (нула QtCharts
  widgets), което би направило теста GUI тест за депрекейтнати типове.
  Фиксът е един и същ едноредов; gap-ът е отбелязан, не платен.