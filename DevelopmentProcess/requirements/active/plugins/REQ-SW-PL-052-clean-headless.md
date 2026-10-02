# REQ-SW-PL-052 — Clean Headless (Option B)

> **Parent:** REQ-SW-PL-051 (core/gui separation)
> **Depends on:** REQ-SW-PL-051 (already implemented Option A)
> **Scope:** NodeRunner headless mode — true clean headless without QtWidgets

## Проблем
Текущият Option A (REQ-SW-PL-051) работи в headless, но:
- Линква `QtWidgets` и създава невидими виджети за Display/VideoOutput нодове
- Binary size ~15-20 MB вместо ~2-3 MB
- Тихий failure — флоу с VideoDisplay/VideoOutput се зареждат, но нищо не се визуализира

## Цел
Наистински чист headless: **`QCoreApplication` + `FrameworkCore` ONLY**, без `QtWidgets`, без невидими виджети.

## Изисквания

### AC1 — Headless Registry ✅
`INodeProvider` получава нов метод:
```cpp
virtual void registerNodesHeadless(NodeDelegateModelRegistry& registry) = 0;
```
Само "чисти" нодове (без widgets) се регистрират в headless mode.

### AC2 — Fail-fast за не-хеадлес флоу ✅
При `NodeRunner --headless --run file.flow`:
- Ако флоуът съдържа нодове, липсващи в headless registry → **error** (не warning)
- Ясно съобщение кои нодове не са поддържани
- Exit code != 0

### AC3 — Headless Engine ✅
`HeadlessEngine` използва `registerNodesHeadless()` вместо `registerNodes()`.

### AC4 — Node Classification ✅
| Headless-compatible | GUI-only (excluded) |
|---|---|
| NumberSource, NumberDisplay, Modulo, ArithmeticLogic | VideoOutput, VideoDisplay, NumberDisplay (widget) |
| AudioSource, AudioSourceObsolete, ConsoleDataModel | DaqDisplay, VideoGLBlitWidget |
| VideoFileSource, StreamSource, CameraSource, VideoEffect | QDevIoDisplayModelObsolete |
| FrameSamplerNode, CustomShaderNode | CustomShaderNode (widget config) |
| PlutoSdrModel, SystemMonitor, Gamepad, GpuMonitor | PlutoSdrWidget, SystemMonitorWidget, etc. |
| NetworkSource, NetworkSink, FilePlayback, FileRecord | All *Widget classes |
| PcapModel, JackDetectModel, GpuMonitorModel, JackDetectModel | |
| FilePlayback, FileRecord, NetworkSource, NetworkSink | |
| DemuxNode, MuxNode (obsolete) | |

### AC5 — Binary Size ⚠️
Headless binary все още линква QtWidgets (през NodeEditorIde зависимост). Binary size ~15-20 MB. Пълно премахване на QtWidgets изисква разделение на NodeEditorIde → NodeEditorIdeCore/Gui (отделен REQ).

### AC6 — Backward Compatibility ✅
Option A (`NodeRunner --run` / GUI runtime) остава непроменен.

### AC7 — Tests ✅
All tests pass (Qt5/Qt6)

## Acceptance Criteria Status

| AC | Description | Status |
|---|---|---|
| 1 | `INodeProvider::registerNodesHeadless()` added + implemented | ✅ |
| 2 | Headless fails fast with clear error for unsupported nodes | ✅ |
| 3 | `HeadlessEngine` uses `registerNodesHeadless()` | ✅ |
| 4 | Node classification documented + implemented | ✅ |
| 5 | Headless binary ≤ 5 MB | ⚠️ Partial (QtWidgets still linked) |
| 6 | Option A (GUI runtime) unchanged | ✅ |
| 7 | All tests pass (Qt5/Qt6) | ✅ |

## Implementation Summary

**Files changed:**
- `src/plugins/common/capabilities/INodeProvider.h` - added `registerNodesHeadless()`
- `src/plugins/demo_nodeditor_nodes_core/DemoNodeEditorNodesCoreObject.{h,cpp}` - implemented headless registry
- `src/frame_work/base/src/engine/HeadlessEngine.{h,cpp}` - uses headless registry + fail-fast validation
- `src/plugins/node_editor_ide/RuntimeShell.{h,cpp}` - GUI runtime fixes (closeEvent, DeInitialize)
- Test flows updated: `video_4views.flow`, `autostart_test.flow`

**Verification:**
- `NodeRunner --headless --run number_graph.flow` → success (exit 0)
- `NodeRunner --headless --run video_4views.flow` → fail with clear error (exit 1)
- `NodeRunner --run video_4views.flow` → GUI runtime works (MDI, 4 VideoOutput widgets)
- All 11 tests pass on Qt5/Qt6

**Known limitation:** Headless binary still links QtWidgets (via NodeEditorIde dependency). Full QtWidgets removal requires NodeEditorIde split (separate REQ).
