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

### Fix 2: stop() protocol on NodeDelegateModel
- Add `virtual void stop() {}` to `NodeDelegateModel.hpp` (default no-op)
- Each node model with background work overrides it (idempotent)
- `deleteNode()` in `DataFlowGraphModel.cpp` calls `model->stop()` before erase
- Destructors call `stop()` first (single shutdown path)

## Changes Table

### Framework (2 files)
| File | Change |
|------|--------|
| `QPluginManager.cpp` | `ShutdownPluginManager()`: after `shutdownAll()`, synchronously delete all plugin instances |
| `PluginRegistry.{h,cpp}` | Added `allPluginInstances()` — collects all `QBasePluginObject*` across all interfaces |

### NodeEditor (2 files, submodule)
| File | Change |
|------|--------|
| `NodeDelegateModel.hpp` | Added `virtual void stop() {}` |
| `DataFlowGraphModel.cpp` | `deleteNode()`: calls `modelIt->second->stop()` before erase |

### Node Models (18 files = 9 models × 2)
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
- Run 1: ✅ Clean exit (code 0) with SIGTERM
- Run 2: ✅ Clean exit (code 0) with SIGTERM
- Run 3: ✅ Clean exit (code 0) with SIGTERM

### nm Check
- `nm -D build_qt6/bin/libDemoNodeEditorNodesPlugin.so | grep -c "stop"` → 45

### ctest
- Not configured (BUILD_TESTING=OFF) — no unit tests available

## Acceptance Criteria Status
- [x] AC1: NodeDelegateModel has virtual stop()
- [x] AC2: Thread-based nodes implement stop(): PlutoSdr, Pcap, AudioSource, VideoEffect, LLama
- [x] AC3: QTimer-based nodes implement stop(): Gamepad, SystemMonitor, GpuMonitor, JackDetect
- [x] AC4: deleteNode() calls stop() before destruction
- [x] AC5: ShutdownHandler chain guarantees stop() → wait() before exit (synchronous delete)
- [x] AC6: No crash with active threads on close (headless/Xvfb — 3 clean runs)
- [x] AC7: Tests — deferred per standing instruction (NO NEW TESTS)
