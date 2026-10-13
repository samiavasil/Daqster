# Node Editor — каталог на нодовете

Родител: [Node Editor](README.md) | [Architecture](../README.md)

Каноничният списък с нодовете, които **`NodeditorNodesCore`** регистрира в
`NodeDelegateModelRegistry` (ground truth:
`src/plugins/node_editor/nodeditor_nodes/core/NodeditorNodesCoreObject.cpp`,
`registerNodes()` за GUI и `registerNodesHeadless()` за headless — двата регистрират
един и същ набор, защото всички модели са widget-free).

**Категорийният ключ е `"<Група>/<Подгрупа>"` и е низът, който се вижда в
палитрата/менюто и се записва в `.flow` файловете** (записът е по `name()`, не по
категория — затова преместването между категории не изисква миграция на сцени).

## Категории

| Категория | Нодове |
|-----------|--------|
| `Sources/General` | `NumberSourceDataModel` |
| `Displays/General` | `NumberDisplayDataModel` |
| `Operators/General` | `ModuloModel`, `ArithmeticLogicModel` |
| `Sources/Audio` | `AudioSourceDataModel`, `JackDetectModel`¹ |
| `Sources/Video` | `CameraSourceNode`, `VideoFileSourceNode`, `StreamSourceNode` |
| `Sources/File` | `FilePlaybackModel` |
| `Sources/Network` | `NetworkSourceModel`, `PcapModel`¹ |
| `Sources/SDR` | `PlutoSdrModel`¹ |
| `Sources/System` | `SystemMonitorModel`¹ |
| `Sources/Gamepad` | `GamepadModel`¹ |
| `Sources/GPU` | `GpuMonitorModel`¹ |
| `Sinks/Video` | `VideoOutputNode` |
| `Sinks/File` | `FileRecordModel` |
| `Sinks/Network` | `NetworkSinkModel` |
| `Processing/Video` | `VideoEffectNode`, `CustomShaderNode`, `FrameSamplerNode` |
| `Displays/DAQ` | `DaqDisplayNode`, `GenericDisplayNode`², `AudioDisplayModelObsolete`, `AudioDisplayAlias`² |
| `AI/LLM` | `LLamaModelDataModel`, `ConsoleDataModel` |
| `Routing` | `DemuxNodeObsolete`, `MuxNodeObsolete` (+ alias-и `DemuxNode`/`MuxNode`) |
| `Obsolete` | `AudioSourceDataModelObsolete` |

¹ **Условен нод** — компилира се/регистрира се само когато съответният CMake define е
включен: `HAVE_LIBIIO` (PlutoSDR), `HAVE_PCAP` (Pcap), `HAVE_SYSTEM_MONITOR`
(System Monitor), `HAVE_GAMEPAD` (Gamepad), `HAVE_NVML` (GPU Monitor),
`HAVE_JACK_DETECT` (Jack Detect). Повечето са Linux-only (`if(NOT WIN32)`).

² **Legacy alias** — тънък наследник само с override на `name()` (напр. `GenericDisplay`,
`AudioDisplay`, `DemuxNode`, `MuxNode`), за да отварят стари записани `.flow` сцени.

> **Брой регистрации:** `registerNodes()` прави **27 безусловни + 6 условни = до 33**
> `registerModel<>()` извиквания. (По-стари версии на този документ казваха „21" —
> невярно.)

## Domain-и на `SampledData`

Нодовете, които емитират/консумират `SampledData`, използват `domain` низ, който
описва смисъла на каналите: `audio`, `system` (CPU/RAM/temp/network), `gamepad`,
`iq` (PlutoSDR I/Q int16), `gpu`, `jack` (audio jack detect). Дисплеят
`DaqDisplayNode` декодира физически (`decodeToPhysical`, `raw × amplitudeScale +
amplitudeOffset`) и строи unit оси от `SampledStreamDescriptor`.

## Свързано

- [Nodeditor Nodes (плъгин)](../../plugins/nodeditor_nodes/README.md) — пълни описания
  на всеки нод, сценарии за употреба и зависимости.
- [Node Data Types](../../plugins/nodeditor_nodes/README.md) — `SampledData`,
  `VideoFrameData`, `EmbeddingData` и др.
- [Flow формат](../formats/flow-format.md) — как нодовете/връзките се записват.
