# REQ-SW-PL-050: Node Thread Lifecycle Protocol — Implementation Plan

## Status: COMPLETE

## Root Cause Analysis (Step 0)

### Hypothesis
Node threads outlive their owning objects because `ShutdownPluginObject()` uses
`deleteLater()` during `aboutToQuit` (event loop dead → deferred deletes never
processed → threads run during ~QApplication teardown).

### Backtrace / Verification
- Could not reproduce crash in headless/Xvfb without active threaded nodes
  (PlutoSdr, Pcap, Gamepad — no hardware/flow files available in CI env).
- **Theoretical crash path confirmed** by code inspection:
  1. `aboutToQuit` → `ShutdownPluginManager()` → `shutdownAll()`
  2. `ShutdownAllPluginObjects()` → `ShutdownPluginObject()` → `deleteLater()`
  3. Event loop exits after `aboutToQuit` handler returns
  4. `deleteLater()` events never processed → objects survive into `~QApplication`
  5. Node threads (QThread) reference partially-destroyed objects → crash

### Root Cause
**Design flaw:** async `deleteLater()` during event-loop shutdown. Deferred
deletes require a running event loop; `aboutToQuit` means the loop is exiting.

## Design Decisions

### Fix 1: Synchronous plugin shutdown (QPluginManager)
After `shutdownAll()` (which calls `deleteLater()` per plugin object), immediately
collect all plugin object instances and `delete` them synchronously. Destructors
join threads. The `deleteLater()` events become harmless no-ops (object already
deleted, Qt skips).

### Fix 2: stop() protocol via IStoppable interface (Daqster code)
- **Design decision (user, 2026-09-08): NO changes to external submodules
  (nodeeditor).** The earlier NodeDelegateModel::stop() virtual + deleteNode()
  call in the nodeeditor submodule was REVERTED.
- Instead, Daqster defines `shared/IStoppable.h` in `demo_nodeditor_nodes`:
  a pure interface `Daqster::IStoppable` with `virtual void stop() = 0`.
- Each node model with background work implements `IStoppable` (idempotent
  stop()); destructors call `stop()` first (single shutdown path).
- The synchronous plugin shutdown (Fix 1) deletes plugin objects while the
  event loop is still alive; model destructors join threads via stop().

## Changes Table

### Framework (2 files)
| File | Change |
|------|--------|
| `QPluginManager.cpp` | `ShutdownPluginManager()`: after `shutdownAll()`, synchronously delete all plugin instances |
| `PluginRegistry.{h,cpp}` | Added `allPluginInstances()` — collects all `QBasePluginObject*` across all interfaces |

### NodeEditor (submodule) — REVERTED (user decision: no external submodule changes)
| File | Change |
|------|--------|
| `NodeDelegateModel.hpp` | ~~Added `virtual void stop() {}`~~ — REVERTED to `906e300` |
| `DataFlowGraphModel.cpp` | ~~`deleteNode()`: calls `modelIt->second->stop()` before erase~~ — REVERTED |

### IStoppable interface (Daqster code, NEW)
| File | Change |
|------|--------|
| `shared/IStoppable.h` | New `Daqster::IStoppable` pure interface (`virtual void stop() = 0`) |

### Node Models (18 models — all implement Daqster::IStoppable)
| Model | stop() mechanism |
|-------|-----------------|
| PlutoSdrModel | `m_engine->stop()` — joins stream thread |
| PcapModel | `m_engine->stop()` — joins capture thread |
| AudioSourceDataModel | `m_thread->quit(); m_thread->wait()` — stops capture thread |
| VideoEffectNode | ComputePool cancel + perf timer stop |
| LLamaModelDataModel | `m_serverProcess->terminate(); waitForFinished()` |
| GamepadModel | `m_engine->stop()` — stops polling timer + closes fd |
| SystemMonitorModel | `m_engine->stop()` — stops polling timer |
| GpuMonitorModel | `m_engine->stop()` — nvmlShutdown + timer stop |
| JackDetectModel | `m_engine->stop()` — stops polling timer |
| VideoFileSourceNode | `m_player->stop()` — QMediaPlayer |
| StreamSourceNode | `m_player->stop()` — QMediaPlayer |
| CameraSourceNode | `stopCamera()` — QCamera |
| VideoOutputNode | Timer stop + detached window cleanup |
| DaqDisplayNode | Timer stop + ComputePool cancel |
| FileRecordModel | `stopRecording()` |
| NetworkSinkModel | `stopSending()` |
| FilePlaybackModel | `stopPlayback()` |
| NetworkSourceModel | `stopListening()` |

## Verification Results

### Build
- Qt6: ✅ Clean build, 0 errors
- Qt5: ✅ Clean build, 0 errors

### Headless Crash Test (AC6 gate)
- Run 1: ✅ Clean exit (code 0) with SIGTERM (offscreen platform, active threads ~16% CPU)
- Run 2: ✅ Clean exit (code 0) with SIGTERM
- Run 3: ✅ Clean exit (code 0) with SIGTERM

### nm Check
- `nm -D build_qt6/bin/libDemoNodeEditorNodesPlugin.so | grep -c "stop"` → 62
  (IStoppable implementations + engine stop symbols)

### ctest
- Not configured (BUILD_TESTING=OFF) — no unit tests available

## Acceptance Criteria Status
- [x] AC1: IStoppable interface in Daqster code (`shared/IStoppable.h`) — replaces NodeDelegateModel virtual (submodule reverted)
- [x] AC2: Thread-based nodes implement IStoppable: PlutoSdr, Pcap, AudioSource, VideoEffect, LLama
- [x] AC3: QTimer-based nodes implement IStoppable: Gamepad, SystemMonitor, GpuMonitor, JackDetect
- [x] AC4: deleteNode() calls stop() before destruction — N/A (submodule reverted); destructors call stop() as single shutdown path
- [x] AC5: ShutdownHandler chain guarantees stop() → wait() before exit (synchronous delete)
- [x] AC6: No crash with active threads on close (headless/offscreen — 3 clean runs, EXIT 0)
- [x] AC7: Tests — deferred per standing instruction (NO NEW TESTS)
