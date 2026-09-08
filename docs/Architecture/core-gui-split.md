# Core/GUI Split Architecture (REQ-SW-PL-051)

## Overview

This document describes the Core/GUI separation pattern implemented in Daqster for REQ-SW-PL-051. The pattern follows the SDRangel model where each node is split into:

- **Core model** — Pure logic, no GUI dependencies, QtCore only
- **GUI widget** — Visual representation, connects to core model via signals/slots

This enables:
- Headless execution (`Daqster --headless --run <flow>`) without QtWidgets/QtGui
- Same flow files work in both GUI and headless modes
- Clean separation of concerns

## Library Structure

```
src/
├── core/                    # DaqsterCore (QtCore only)
│   ├── plugin/              # QPluginManager, QPluginInterface, QBasePluginObject
│   ├── platform/            # ShutdownHandler (Unix/Windows)
│   ├── logging/             # LogManager, LogCategories
│   ├── process/             # QProcessManager
│   ├── perf/                # PerfProfiler, ProcessCpu
│   ├── capabilities/        # INodeProvider, IStoppable, IStartable
│   ├── data/                # All NodeData types (TextData, FloatData, VideoFrameData, etc.)
│   └── engine/              # HeadlessEngine, FlowLoader
├── gui/                     # DaqsterGui (QtWidgets)
│   ├── plugin/              # QPluginManagerGui, QPluginListView, DebugConsoleWidget
│   ├── editor/              # NodeEditorWidget, RuntimeShell, FlowUiSection
│   ├── builtin_nodes/       # NumberSource, NumberDisplay, Modulo, ArithmeticLogic
│   └── library/             # Merged NodeEditorLibrary content
└── plugins/
    ├── demo_nodeditor_nodes_core/   # Core node models (no widgets)
    ├── demo_nodeditor_nodes_gui/    # GUI widgets + NodeWidgetFactory
    └── node_editor_ide/             # IDE shell (depends on DaqsterGui)
```

## Node Split Pattern

### Core Model (in `_core` plugin)

```cpp
// AudioSourceDataModel.h (core)
class AudioSourceDataModel : public QtNodes::NodeDelegateModel,
                             public Daqster::IStoppable,
                             public Daqster::IStartable
{
    Q_OBJECT
public:
    // ... NodeDelegateModel overrides ...
    
    // Core logic only - NO QtWidgets includes
    void stop() override;      // IStoppable
    void start() override;     // IStartable
    
    // Returns nullptr in core plugin
    QWidget* embeddedWidget() override { return nullptr; }
    
private:
    QThread* m_thread = nullptr;
    MicCaptureWorker* m_worker = nullptr;
    // NO widget member!
};
```

### GUI Widget (in `_gui` plugin)

```cpp
// AudioSourceDataModelUI.h (gui)
class AudioSourceDataModelUI : public QWidget
{
    Q_OBJECT
public:
    explicit AudioSourceDataModelUI(AudioSourceDataModel* model, QWidget* parent = nullptr);
    
Q_SIGNALS:
    void startRequested();
    void stopRequested();
    void configChanged();
    
private:
    AudioSourceDataModel* m_model;
    QPushButton* m_startBtn;
    QPushButton* m_stopBtn;
    // ... UI controls ...
};
```

### NodeWidgetFactory (in `_gui` plugin)

```cpp
// NodeWidgetFactory.cpp
void DemoNodeEditorNodesGuiObject::Initialize()
{
    m_widgetFactory = new NodeWidgetFactory(this);
    
    // Register widget creators for each core model
    m_widgetFactory->registerWidgetCreator("AudioSource", 
        [](QtNodes::NodeDelegateModel* model) {
            auto* core = qobject_cast<AudioSourceDataModel*>(model);
            return new AudioSourceDataModelUI(core);
        });
    
    m_widgetFactory->registerWidgetCreator("DaqDisplay",
        [](QtNodes::NodeDelegateModel* model) {
            auto* core = qobject_cast<DaqDisplayNode*>(model);
            return new DaqDisplayNodeUI(core);
        });
    
    // ... register all 24+ node types ...
}
```

### Editor Integration

```cpp
// NodeEditorWidget::onNodeCreated() or RuntimeShell::arrangeWorkspaces()
void RuntimeShell::arrangeWorkspaces(const FlowUi::UiSection& ui)
{
    // ... existing code ...
    
    // Get widget from factory instead of model->embeddedWidget()
    auto* factory = getWidgetFactory();  // From demo_nodeditor_nodes_gui plugin
    QWidget* widget = factory ? factory->createWidget(model) : model->embeddedWidget();
    
    if (widget) {
        node->setWidgetEmbedded(false);
        // ... create MDI sub-window with widget ...
    }
}
```

## Headless Mode

### DaqsterHeadless Executable

```bash
# Build
./scripts/build.sh qt5    # or qt6

# Run
./build_qt5/bin/DaqsterHeadless --run my_flow.flow
```

**Verification:**
```bash
# Should be EMPTY (no QtWidgets/QtGui)
ldd build_qt5/bin/DaqsterHeadless | grep -E "Qt5Widgets|Qt5Gui"

# Should be EMPTY
ldd build_qt5/bin/libDaqsterCore.so | grep -E "Qt5Widgets|Qt5Gui"
```

### HeadlessEngine

```cpp
// HeadlessEngine::loadFlow()
bool HeadlessEngine::loadFlow(const QString& flowPath)
{
    // 1. Register nodes via INodeProvider (core plugins only)
    registerNodes();
    
    // 2. Build graph model (no scene/view)
    buildGraphModel();
    
    // 3. Parse .flow JSON, instantiate models
    parseFlowFile(flowPath);
    
    // 4. Connect nodes
    connectNodes(connectionsJson);
    
    // 5. Auto-start nodes (IStartable::start())
    autoStartNodes();
    
    return true;
}
```

## Build Configuration

### CMake Options

```cmake
# Root CMakeLists.txt
option(DAQSTER_BUILD_GUI "Build GUI components" ON)

# Always built
add_subdirectory(src/core)
add_subdirectory(src/plugins/demo_nodeditor_nodes_core)
add_subdirectory(src/apps/DaqsterHeadless)

# Only when DAQSTER_BUILD_GUI=ON
if(DAQSTER_BUILD_GUI)
    add_subdirectory(src/gui)
    add_subdirectory(src/plugins/node_editor_ide)
    add_subdirectory(src/plugins/demo_nodeditor_nodes_gui)
    add_subdirectory(src/apps/Daqster)
endif()
```

### Dependency Graph

```
DaqsterCore (QtCore)
    ↑
DaqsterGui (QtWidgets) ──────→ DaqsterCore
    ↑
demo_nodeditor_nodes_gui ─────→ DaqsterGui + DaqsterCore + demo_nodeditor_nodes_core
    ↑
node_editor_ide ──────────────→ DaqsterGui + DaqsterCore
    ↑
Daqster (GUI app) ────────────→ node_editor_ide + DaqsterGui + DaqsterCore

DaqsterHeadless ──────────────→ DaqsterCore (ONLY)
```

## Migration Guide for Existing Nodes

### Step 1: Move Core Logic to `_core` Plugin

1. Move `*Model.h/.cpp` to `demo_nodeditor_nodes_core/Sources/...`
2. Remove all QtWidgets includes (`#include <QWidget>`, `<QLabel>`, etc.)
3. Change `embeddedWidget()` to return `nullptr`
4. Add `IStoppable`/`IStartable` if needed
5. Keep only QtCore/QtNodes includes

### Step 2: Create GUI Widget in `_gui` Plugin

1. Create `*Widget.h/.cpp/.ui` in `demo_nodeditor_nodes_gui/Sources/...`
2. Widget constructor takes core model pointer
3. Connect widget signals to core model slots
4. Register in `NodeWidgetFactory`

### Step 3: Update CMakeLists.txt

**Core plugin:**
```cmake
create_plugin(DemoNodeEditorNodesCore
    SOURCES ${CORE_SOURCES}
    REQUIRES_LIBRARIES
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui      # Only for QVideoFrame etc.
        QtNodes
        DaqsterCore
)
```

**GUI plugin:**
```cmake
create_plugin(DemoNodeEditorNodesGui
    SOURCES ${GUI_WIDGETS} ${FACTORY_SOURCES}
    REQUIRES_LIBRARIES
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        Qt${QT_VERSION_MAJOR}::Widgets
        QtNodes
        DaqsterCore
        DaqsterGui
        DemoNodeEditorNodesCore
)
```

## Representative Models (Phase 1)

The following 3 models were split first to verify the pattern end-to-end:

| Model | Type | Interfaces | Notes |
|-------|------|------------|-------|
| `AudioSource` | Source | `IStartable`, `IStoppable` | Worker thread, audio capture |
| `DaqDisplay` | Display | `IStoppable` | QtCharts, compute pool |
| `VideoOutput` | Display | `IStoppable` | OpenGL, detached window |

## Remaining Models (Phase 2)

After verifying the pattern with the 3 representatives, the remaining 21+ models are split in batch:

- **Sources:** CameraSource, VideoFileSource, StreamSource, FilePlayback, NetworkSource, PlutoSDR, SystemMonitor, Gamepad, GpuMonitor, JackDetect, PcapCapture, LLamaSource
- **Sinks:** FileRecord, NetworkSink
- **Processing:** VideoEffect, CustomShader, FrameSampler
- **Displays:** (GenericDisplay - obsolete)
- **Routing:** Demux, Mux (obsolete)

## Testing

### Headless Smoke Test

```bash
# Create a simple test flow
cat > test.flow <<'EOF'
{
  "nodes": [
    {"id": 0, "internal-data": {"model-name": "NumberSource"}, "position": [100, 100]},
    {"id": 1, "internal-data": {"model-name": "NumberDisplay"}, "position": [400, 100]}
  ],
  "connections": [
    {"outNodeId": 0, "outPortIndex": 0, "inNodeId": 1, "inPortIndex": 0}
  ]
}
EOF

# Run headless
./build_qt5/bin/DaqsterHeadless --run test.flow
# Should exit cleanly after processing
```

### GUI Regression Test

```bash
# Run GUI app
./build_qt5/bin/Daqster

# Verify:
# - All 24+ nodes appear in palette
# - Drag-drop creates nodes with widgets
# - Connections work
# - Runtime mode (--run) works
# - Presentation mode (F11) works
# - MDI workspaces work
# - AutoStart from flow UI section works
```

## Future Extensions

### REST API (Optional)

The headless engine can be extended with a REST API for remote control:

```cpp
// Future: HeadlessEngine + REST server
class HeadlessServer : public HeadlessEngine
{
    // HTTP endpoints:
    // POST /flow/load     - Load flow file
    // POST /flow/start    - Auto-start nodes
    // POST /flow/stop     - Stop all nodes
    // GET  /flow/status   - Get node statuses
    // WS   /flow/events   - Real-time data stream
};
```

This would enable:
- Remote flow deployment
- Integration with CI/CD pipelines
- Web-based monitoring dashboards
- Multi-instance orchestration

## References

- SDRangel architecture: `sdrangel` (GUI) vs `sdrangelsrv` (headless)
- Daqster runtime mode: `docs/Architecture/runtime-mode-architecture.md` (Phase 2)
- Plugin pattern: `DevelopmentProcess/AGENT-KNOWLEDGE.md` §7