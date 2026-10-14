# GUI Layout Editor (Design Mode) Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Build a visual Layout Designer Mode inside NodeEditor IDE to design v2 layout trees (split/grid/tab/panel + docks) by drag-drop, using existing LayoutTree/LayoutEngine, serialize to `.flow` "ui" section (version 2).

**Architecture:** 
- Add "Layout Designer Mode" toggle (F6 / toolbar) to NodeEditorIde/NodeEditorWidget. In designer mode, hide the graph canvas, show layout editor UI (central area + side dock zone + available widgets panel).
- Designer works directly on `FlowUi::LayoutSection` (v2). For v2 save, serialize `LayoutSection::toJson()` with version 2. For load, if ui.version>=2 parse LayoutSection and restore.
- Use `LayoutEngine::buildWorkspace()` for preview/runtime-style WYSIWYG; use `LayoutEngine::captureWorkspace()` to extract live hierarchy back to LayoutSection on save.
- Available widgets come from graph nodes with embedded widgets (deembeddable nodes). Provider maps nodeId to current node widget.
- Start minimal: create containers (split H/V, grid 2x2 minimal), add panels to cells, add docks, drag nodes from Available panel into containers/docks, capture to LayoutSection, save to flow.

**Tech Stack:** Qt5/Qt6 (Widgets), QtNodes, FlowUi::LayoutTree/FlowEngine.

---

### Task 1: Read existing layout code to anchor implementation

**Files:**
- Read: `/mnt/Builder/Projects/samiavasil/daqster/src/plugins/node_editor/node_editor_ide/LayoutTree.h`
- Read: `/mnt/Builder/Projects/samiavasil/daqster/src/plugins/node_editor/node_editor_ide/LayoutTree.cpp`
- Read: `/mnt/Builder/Projects/samiavasil/daqster/src/plugins/node_editor/node_editor_ide/LayoutEngine.h`
- Read: `/mnt/Builder/Projects/samiavasil/daqster/src/plugins/node_editor/node_editor_ide/LayoutEngine.cpp`
- Read: `/mnt/Builder/Projects/samiavasil/daqster/src/plugins/node_editor/node_editor_ide/FlowUiSection.h`
- Read: `/mnt/Builder/Projects/samiavasil/daqster/src/plugins/node_editor/node_editor_ide/FlowUiSection.cpp`
- Read: `/mnt/Builder/Projects/samiavasil/daqster/src/plugins/node_editor/node_editor_ide/NodeEditorIdeObject.cpp`
- Read: `/mnt/Builder/Projects/samiavasil/daqster/src/plugins/node_editor/node_editor_ide/NodeEditorWidget.cpp`
- Read: `/mnt/Builder/Projects/samiavasil/daqster/src/plugins/runtime_host/gui/RuntimeShell.cpp` (for runtime v2 wiring reference)
- Read: `/mnt/Builder/Projects/samiavasil/daqster/tests/plugins/node_editor_ide/test_flow_fixtures.cpp`

**Step 1: Understand v2 model and engine**
- Parse LayoutTree structure (split/grid/tab/panel, docks, WorkspaceLayout, LayoutSection)
- Understand LayoutEngine::buildWorkspace/provider contract and captureWorkspace
- Understand FlowUiSection v1 vs v2 split (note: LayoutSection is separate in v2 path)

**Step 2: Understand how UI section is saved/loaded in IDE**
- See how NodeEditorIdeObject captures/applies UiSection (v1). Need to extend to v2 with version switch.

**Step 3: Verify tests related to v2 fixtures**
- See video_4views_grid.flow expectations and v2 layout tests

**Expected:** Clear mental model of v1->v2 transition points.

**Step 5: Commit (if any doc notes, but usually none)** - skip if no changes.

### Task 2: Add LayoutDesignerMode enum/state to NodeEditorWidget

**Files:**
- Modify: `/mnt/Builder/Projects/samiavasil/daqster/src/plugins/node_editor/node_editor_ide/NodeEditorWidget.h`
- Modify: `/mnt/Builder/Projects/samiavasil/daqster/src/plugins/node_editor/node_editor_ide/NodeEditorWidget.cpp`

**Step 1: Write failing test (conceptual)**
- Add test understanding in mind: designer mode should hide canvas and show designer UI.

**Step 2: Add mode enum**
Add to NodeEditorWidget.h:
```cpp
enum class EditorMode { Normal, LayoutDesigner };
Q_PROPERTY(EditorMode mode READ mode WRITE setMode NOTIFY modeChanged)
EditorMode mode() const;
void setMode(EditorMode m);
signals: void modeChanged();
private: EditorMode m_mode{EditorMode::Normal};
```

**Step 3: Implement setter**
In cpp: toggle visibility of graph view/canvas vs designer panel. Keep existing API intact.

**Step 4: Verify compile**
```bash
cd /mnt/Builder/Projects/samiavasil/daqster/build_qt5 && make -j4 2>&1 | tail -30
```

**Step 5: Commit**
```bash
cd /mnt/Builder/Projects/samiavasil/daqster
git add src/plugins/node_editor/node_editor_ide/NodeEditorWidget.h src/plugins/node_editor/node_editor_ide/NodeEditorWidget.cpp
git commit -m "feat(layout-editor): add EditorMode to NodeEditorWidget"
```

### Task 3: Create LayoutDesignerPanel widget (core UI skeleton)

**Files:**
- Create: `/mnt/Builder/Projects/samiavasil/daqster/src/plugins/node_editor/node_editor_ide/LayoutDesignerPanel.h`
- Create: `/mnt/Builder/Projects/samiavasil/daqster/src/plugins/node_editor/node_editor_ide/LayoutDesignerPanel.cpp`
- Modify: `/mnt/Builder/Projects/samiavasil/daqster/src/plugins/node_editor/node_editor_ide/CMakeLists.txt`

**Step 1: Write failing test (headless sanity)**
Create minimal test idea: LayoutSection roundtrip with empty section. (optional; can rely on existing tests)

**Step 2: Define LayoutDesignerPanel**
Header: QFrame/QWidget with:
- Toolbar (Add Split H/V, Add Grid 2x2, Add Tab, Add Dock L/R/T/B, Capture Current, Save Layout)
- Splitter: left Available Widgets panel (QListWidget), center Designer area (QStackedWidget/placeholder QMainWindow container), right maybe properties
- Designer area holds preview QMainWindow built via LayoutEngine (or editable container)
- Tracks current LayoutSection + selected node/container

**Step 3: Implement skeleton**
Build basic layout, no heavy drag-drop yet. Store m_layoutSection (v2).

**Step 4: Add to CMakeLists.txt**
Add LayoutDesignerPanel.cpp/h to node_editor_ide sources.

**Step 5: Verify compile**
```bash
cd /mnt/Builder/Projects/samiavasil/daqster/build_qt5 && make -j4 2>&1 | tail -40
```

**Step 6: Commit**
```bash
cd /mnt/Builder/Projects/samiavasil/daqster
git add src/plugins/node_editor/node_editor_ide/LayoutDesignerPanel.h src/plugins/node_editor/node_editor_ide/LayoutDesignerPanel.cpp src/plugins/node_editor/node_editor_ide/CMakeLists.txt
git commit -m "feat(layout-editor): add LayoutDesignerPanel skeleton"
```

### Task 4: Wire LayoutDesignerPanel into NodeEditorWidget and NodeEditorIdeObject

**Files:**
- Modify: `/mnt/Builder/Projects/samiavasil/daqster/src/plugins/node_editor/node_editor_ide/NodeEditorWidget.h`
- Modify: `/mnt/Builder/Projects/samiavasil/daqster/src/plugins/node_editor/node_editor_ide/NodeEditorWidget.cpp`
- Modify: `/mnt/Builder/Projects/samiavasil/daqster/src/plugins/node_editor/node_editor_ide/NodeEditorIdeObject.h`
- Modify: `/mnt/Builder/Projects/samiavasil/daqster/src/plugins/node_editor/node_editor_ide/NodeEditorIdeObject.cpp`

**Step 1: Add panel member to NodeEditorWidget**
Add LayoutDesignerPanel* m_designerPanel; create/show/hide on mode change. Keep canvas visible only in Normal.

**Step 2: Add F6 toggle in NodeEditorIdeObject (or NodeEditorWidget action)**
Add action "Layout Designer Mode" (F6), checkable. Connect to setMode.

**Step 3: Minimal wiring**
When entering designer mode, initialize LayoutSection from current flow UI (if ui.version==2 use it, else start empty single workspace with empty root or keep as-is).

**Step 4: Verify compile**
```bash
cd /mnt/Builder/Projects/samiavasil/daqster/build_qt5 && make -j4 2>&1 | tail -20
```

**Step 5: Commit**
```bash
cd /mnt/Builder/Projects/samiavasil/daqster
git add src/plugins/node_editor/node_editor_ide/NodeEditorWidget.* src/plugins/node_editor/node_editor_ide/NodeEditorIdeObject.*
git commit -m "feat(layout-editor): wire designer mode toggle and panel into IDE"
```

### Task 5: Build preview from LayoutSection using LayoutEngine

**Files:**
- Modify: `/mnt/Builder/Projects/samiavasil/daqster/src/plugins/node_editor/node_editor_ide/LayoutDesignerPanel.cpp`
- Modify: `/mnt/Builder/Projects/samiavasil/daqster/src/plugins/node_editor/node_editor_ide/LayoutDesignerPanel.h`

**Step 1: Write failing test idea**
Given empty LayoutSection (v2, one workspace empty root?) — buildWorkspace should produce QMainWindow or return null gracefully. Existing LayoutEngine tests cover tree/engine; add minimal panel behavior test later if needed.

**Step 2: Implement preview refresh**
Add method refreshPreview():
- Take m_layoutSection, build preview by creating QMainWindow via LayoutEngine::buildWorkspace for each workspace? Start with single workspace.
- Provider: for nodeId return node widget from graph model/editor widget (deembed conceptually — we want live widget instance). If widget not available, return nullptr (panel skipped per engine behavior).
- Replace current preview container content with built window/widget.

**Step 3: Handle empty root**
If root is empty panel with invalid id? Engine builds panel only if nodeId valid and provider returns widget. For empty layout, buildWorkspace on empty root may return central empty widget? Check LayoutEngine: buildNode for Panel returns nullptr if nodeId invalid or provider returns nullptr. So central may be null — handle by showing placeholder.

**Step 4: Verify compile + quick manual check**
```bash
cd /mnt/Builder/Projects/samiavasil/daqster/build_qt5 && make -j4 2>&1 | tail -15
```

**Step 5: Commit**
```bash
cd /mnt/Builder/Projects/samiavasil/daqster
git add src/plugins/node_editor/node_editor_ide/LayoutDesignerPanel.*
git commit -m "feat(layout-editor): preview v2 layout via LayoutEngine::buildWorkspace"
```

### Task 6: Available Widgets panel (list deembeddable nodes)

**Files:**
- Modify: `/mnt/Builder/Projects/samiavasil/daqster/src/plugins/node_editor/node_editor_ide/LayoutDesignerPanel.h`
- Modify: `/mnt/Builder/Projects/samiavasil/daqster/src/plugins/node_editor/node_editor_ide/LayoutDesignerPanel.cpp`

**Step 1: Populate list**
Show all nodeIds from graph model that have embedded widget / are deembeddable? Check NodeGraphicsObject::isWidgetEmbedded() or graphModel node existence + editor can provide widget. List by node id + label/model name.

**Step 2: Enable drag from list**
Set QListWidget to support drag (QListWidget::setDragEnabled(true), provide MIME data with nodeId).

**Step 3: Keep in sync with graph**
Refresh on mode enter or when nodes added/removed (optional minimal: refresh on show).

**Step 4: Verify compile**
```bash
cd /mnt/Builder/Projects/samiavasil/daqster/build_qt5 && make -j4 2>&1 | tail -10
```

**Step 5: Commit**
```bash
cd /mnt/Builder/Projects/samiavasil/daqster
git add src/plugins/node_editor/node_editor_ide/LayoutDesignerPanel.*
git commit -m "feat(layout-editor): available widgets list with drag support"
```

### Task 7: Minimal editing — create containers and assign panels to grid cells

**Files:**
- Modify: `/mnt/Builder/Projects/samiavasil/daqster/src/plugins/node_editor/node_editor_ide/LayoutDesignerPanel.h`
- Modify: `/mnt/Builder/Projects/samiavasil/daqster/src/plugins/node_editor/node_editor_ide/LayoutDesignerPanel.cpp`

**Goal:** Support minimal operations:
- Add Split H/V: create LayoutNode::Split with orientation, attach as root (or to selected container) — start simple: replace root with split containing two children (empty panels or keep existing)
- Add Grid 2x2: create grid root with 2x2 cells all nullopt
- Allow dropping nodeId from Available list onto a grid cell area? Minimal: when root is grid, provide UI to assign node to specific cell (e.g. right-click on cell in preview? Or simple buttons). Or make grid cells droppable in preview.

**Step 1: Add toolbar actions wiring**
Connect "Add Split H/V", "Add Grid 2x2" to modify m_layoutSection.root and refreshPreview().

**Step 2: Keep LayoutSection in sync**
Ensure workspaces[0].root is set. For minimal single workspace.

**Step 3: Simple assignment to grid cells**
Add small helper: set cell (r,c) to nodeId if dropped on conceptual cell. Since preview is built by LayoutEngine, we can overlay drop zones on grid cells? Or alternatively provide a simple grid editor panel (separate) for minimal first pass.

**Minimal pragmatic approach:** add a "Grid Editor" subpanel when root is Grid — shows 2x2 buttons/drop targets. On drop from Available list onto button, set m_layoutSection.workspaces[0].root.cells[...] = nodeId; refreshPreview().

**Step 4: Verify compile**
```bash
cd /mnt/Builder/Projects/samiavasil/daqster/build_qt5 && make -j4 2>&1 | tail -15
```

**Step 5: Commit**
```bash
cd /mnt/Builder/Projects/samiavasil/daqster
git add src/plugins/node_editor/node_editor_ide/LayoutDesignerPanel.*
git commit -m "feat(layout-editor): basic container creation and grid cell assignment"
```

### Task 8: Docks — add Left/Right/Top/Bottom docks

**Files:**
- Modify: `/mnt/Builder/Projects/samiavasil/daqster/src/plugins/node_editor/node_editor_ide/LayoutDesignerPanel.h`
- Modify: `/mnt/Builder/Projects/samiavasil/daqster/src/plugins/node_editor/node_editor_ide/LayoutDesignerPanel.cpp`

**Step 1: Toolbar actions for docks**
Add actions: Add Dock Left/Right/Top/Bottom. On triggered, create DockLayout with default area and no node assigned yet.

**Step 2: Assign node to dock**
Allow dropping node from Available list onto dock in preview? Or provide dock list panel with "Assign Node" / drop target. Minimal: add "Docks" list (shows existing docks with area/title/node). Support drop onto dock entry → set dock.node = dropped nodeId; refreshPreview().

**Step 3: Update preview on dock changes**
LayoutEngine::buildWorkspace already renders docks when provider returns widget for dock.node. So setting dock.node is enough if provider works.

**Step 4: Verify compile**
```bash
cd /mnt/Builder/Projects/samiavasil/daqster/build_qt5 && make -j4 2>&1 | tail -10
``**

**Step 5: Commit**
```bash
cd /mnt/Builder/Projects/samiavasil/daqster
git add src/plugins/node_editor/node_editor_ide/LayoutDesignerPanel.*
git commit -m "feat(layout-editor): add docks with node assignment"
```

### Task 9: Capture live layout to LayoutSection (captureWorkspace)

**Files:**
- Modify: `/mnt/Builder/Projects/samiavasil/daqster/src/plugins/node_editor/node_editor_ide/LayoutDesignerPanel.h`
- Modify: `/mnt/Builder/Projects/samiavasil/daqster/src/plugins/node_editor/node_editor_ide/LayoutDesignerPanel.cpp`

**Step 1: Implement "Capture Current"**
When clicked, get the preview QMainWindow built by LayoutEngine (or store it). Call `FlowUi::LayoutEngine::captureWorkspace(previewWindow)` → returns `WorkspaceLayout`. Replace m_layoutSection.workspaces[0] with captured one (preserve id/name). Refresh preview and show success/log.

**Step 2: Handle edge cases**
If preview not built or null, no-op with warning.

**Step 3: Verify compile**
```bash
cd /mnt/Builder/Projects/samiavasil/daqster/build_qt5 && make -j4 2>&1 | tail -10
```

**Step 4: Commit**
```bash
cd /mnt/Builder/Projects/samiavasil/daqster
git add src/plugins/node_editor/node_editor_ide/LayoutDesignerPanel.*
git commit -m "feat(layout-editor): capture live preview to LayoutSection"
```

### Task 10: Extend NodeEditorIdeObject to save/load v2 layout in .flow

**Files:**
- Modify: `/mnt/Builder/Projects/samiavasil/daqster/src/plugins/node_editor/node_editor_ide/NodeEditorIdeObject.h`
- Modify: `/mnt/Builder/Projects/samiavasil/daqster/src/plugins/node_editor/node_editor_ide/NodeEditorIdeObject.cpp`

**Step 1: Write failing test (expectation)**
Check existing behavior: saveSceneToFile writes ui version 1. Need to support writing version 2 when designer has LayoutSection or when user wants v2.

**Step 2: Add LayoutSection storage to NodeEditorIdeObject**
Add member: `FlowUi::LayoutSection m_layoutSectionV2; bool m_hasLayoutV2{false};`
Add getters/setters as needed.

**Step 3: Load path (loadSceneFromFile)**
Current: extracts `UiSection` (v1). Modify to:
- Parse ui JSON. If `ui["version"].toInt(1) >= 2` → parse `FlowUi::LayoutSection::fromJson(uiJson)`, store m_hasLayoutV2=true + m_layoutSectionV2, do NOT convert to v1. Keep ui removed before graph load as before.
- Else parse v1 UiSection as is.

**Step 4: Save path (saveSceneToFile)**
When m_hasLayoutV2 true, serialize `m_layoutSectionV2.toJson()` into `sceneJson["ui"]` with version 2 (LayoutSection writes version 2). Otherwise write v1 UiSection as currently.

**Step 5: Apply/restore in IDE (normal mode)**
For v2, IDE needs to restore layout. Two options: can use LayoutEngine to build workspace and embed? Or keep current v1 behavior for IDE normal view? Start minimal: if v2 present, store it; runtime will use v2 (Task 11). IDE normal mode may continue with canvas; designer mode uses stored v2.

**Step 6: Verify compile**
```bash
cd /mnt/Builder/Projects/samiavasil/daqster/build_qt5 && make -j4 2>&1 | tail -15
```

**Step 7: Commit**
```bash
cd /mnt/Builder/Projects/samiavasil/daqster
git add src/plugins/node_editor/node_editor_ide/NodeEditorIdeObject.*
git commit -m "feat(layout-editor): support v2 LayoutSection load/save in NodeEditorIdeObject"
```

### Task 11: Wire v2 layout in RuntimeShell (runtime host)

**Files:**
- Modify: `/mnt/Builder/Projects/samiavasil/daqster/src/plugins/runtime_host/gui/RuntimeShell.h`
- Modify: `/mnt/Builder/Projects/samiavasil/daqster/src/plugins/runtime_host/gui/RuntimeShell.cpp`
- Modify: `/mnt/Builder/Projects/samiavasil/daqster/src/plugins/runtime_host/gui/CMakeLists.txt` (ensure LayoutTree/FlowUiSection available if needed)

**Reference:** git lost-found showed old RuntimeShell had useV2Layout + arrangeLayout using LayoutEngine. Current develop has only v1 arrangeWorkspaces.

**Step 1: Add v2 support in loadFlow/applySceneJson**
In applySceneJson/loadFlow: parse ui JSON. If ui.version >= 2:
- Parse `FlowUi::LayoutSection layoutSection = FlowUi::LayoutSection::fromJson(uiJson); useV2 = true;`
- Else parse v1 UiSection as now.

**Step 2: Implement arrangeLayout using LayoutEngine**
Add method `void arrangeLayout(const FlowUi::LayoutSection& layout)` similar to lost-found version:
- Clear central widget layout
- Build tab widget per workspace (or single) and for each workspace call `FlowUi::LayoutEngine::buildWorkspace(ws, provider)`
- Provider returns deembedded widget for nodeId: reuse existing deembed logic (find node widget via editor widget/graph model)

**Step 3: Route to v2 on load**
After applySceneJson loads graph, if useV2 arrangeLayout(layoutSection) else arrangeWorkspaces(uiSection). Also handle autoStart for v2 (can iterate layoutSection.nodes for autoStart flags).

**Step 4: Add includes**
Include LayoutTree.h/LayoutEngine.h in RuntimeShell.cpp if not already. CMake: node_editor_ide library is linked? Check RuntimeHostGuiPlugin links to NodeEditorLibrary/NodeEditorIde objects? Need to ensure LayoutTree symbols available.

**Step 5: Verify compile**
```bash
cd /mnt/Builder/Projects/samiavasil/daqster/build_qt5 && make -j4 2>&1 | tail -30
cd /mnt/Builder/Projects/samiavasil/daqster/build_qt6 && make -j4 2>&1 | tail -15
```

**Step 6: Commit**
```bash
cd /mnt/Builder/Projects/samiavasil/daqster
git add src/plugins/runtime_host/gui/RuntimeShell.*
git commit -m "feat(runtime): add v2 layout support via LayoutEngine::buildWorkspace"
```

### Task 12: Connect Designer Panel to NodeEditorIde for save/load

**Files:**
- Modify: `/mnt/Builder/Projects/samiavasil/daqster/src/plugins/node_editor/node_editor_ide/LayoutDesignerPanel.h`
- Modify: `/mnt/Builder/Projects/samiavasil/daqster/src/plugins/node_editor/node_editor_ide/LayoutDesignerPanel.cpp`
- Modify: `/mnt/Builder/Projects/samiavasil/daqster/src/plugins/node_editor/node_editor_ide/NodeEditorIdeObject.cpp`
- Modify: `/mnt/Builder/Projects/samiavasil/daqster/src/plugins/node_editor/node_editor_ide/NodeEditorIdeObject.h`

**Step 1: Expose saving v2 from designer**
LayoutDesignerPanel needs access to current graph/editor to build provider and to persist. Add signals/slots or access to NodeEditorIdeObject/NodeEditorWidget.

**Step 2: When entering designer mode, load current v2 if exists**
From NodeEditorIdeObject, if hasLayoutV2 pass m_layoutSectionV2 to panel, else create default empty v2 section (single workspace, empty root).

**Step 3: Save action in designer**
On "Save Layout" in panel: call captureWorkspace if preview exists, store in IdeObject (m_layoutSectionV2 = current, m_hasLayoutV2=true). Then trigger normal save (saveSceneToFile) which will write v2 ui.

**Step 4: Verify compile**
```bash
cd /mnt/Builder/Projects/samiavasil/daqster/build_qt5 && make -j4 2>&1 | tail -10
``**

**Step 5: Commit**
```bash
cd /mnt/Builder/Projects/samiavasil/daqster
git add src/plugins/node_editor/node_editor_ide/LayoutDesignerPanel.* src/plugins/node_editor/node_editor_ide/NodeEditorIdeObject.*
git commit -m "feat(layout-editor): connect designer to IDE save/load of v2 layout"
```

### Task 13: Basic drag-drop between Available list and grid/docks (minimal)

**Files:**
- Modify: `/mnt/Builder/Projects/samiavasil/daqster/src/plugins/node_editor/node_editor_ide/LayoutDesignerPanel.cpp`
- Modify: `/mnt/Builder/Projects/samiavasil/daqster/src/plugins/node_editor/node_editor_ide/LayoutDesignerPanel.h`

**Goal:** Minimal usable interaction. Grid cells: add droppable buttons/labels in grid editor subpanel. Docks list: make dock entries accept drops. On drop set assignment, refreshPreview().

**Step 1: Grid assignment UI**
When root type is Grid, show 2x2 (or R×C) grid of drop targets (QPushButton/QLabel with AcceptDrops). Set mime data "nodeId".

**Step 2: Docks assignment**
Show QListWidget of docks; set each item droppable or use eventFilter. On drop, find dock by index and set node.

**Step 3: Refresh preview after assignment**
Rebuild preview with updated LayoutSection.

**Step 4: Verify compile + smoke**
```bash
cd /mnt/Builder/Projects/samiavasil/daqster/build_qt5 && make -j4 2>&1 | tail -5
```

**Step 5: Commit**
```bash
cd /mnt/Builder/Projects/samiavasil/daqster
git add src/plugins/node_editor/node_editor_ide/LayoutDesignerPanel.*
git commit -m "feat(layout-editor): minimal drag-drop for grid cells and docks"
```

### Task 14: Add Split/Tab container creation (minimal)

**Files:**
- Modify: `/mnt/Builder/Projects/samiavasil/daqster/src/plugins/node_editor/node_editor_ide/LayoutDesignerPanel.cpp`
- Modify: `/mnt/Builder/Projects/samiavasil/daqster/src/plugins/node_editor/node_editor_ide/LayoutDesignerPanel.h`

**Step 1: Split H/V**
Create LayoutNode::Split with orientation Horizontal/Vertical. Initialize children with two empty panels? (nodeId Invalid) or keep existing root as one child. Minimal: set root = split with children[0]=current-root-as-panel? Or start fresh. For minimal, if root is panel with nodeId, can wrap.

**Step 2: Tab**
Create tab root with children = [panel for selected node or empty]. Minimal: allow creating tab and assigning children.

**Step 3: Refresh preview**
After structural change, refreshPreview().

**Step 4: Verify compile**
```bash
cd /mnt/Builder/Projects/samiavasil/daqster/build_qt5 && make -j4 2>&1 | tail -5
``

**Step 5: Commit**
```bash
cd /mnt/Builder/Projects/samiavasil/daqster
git add src/plugins/node_editor/node_editor_ide/LayoutDesignerPanel.*
git commit -m "feat(layout-editor): add split and tab container creation"
```

### Task 15: Smoke tests + manual verification

**Files:**
- Read/verify: existing tests `tests/plugins/node_editor_ide/test_layout_tree.cpp`, `test_layout_engine.cpp`, `test_flow_fixtures.cpp`

**Step 1: Compile all node_editor_ide tests**
```bash
cd /mnt/Builder/Projects/samiavasil/daqster/build_qt5 && make -j4 2>&1 | tail -30
ctest --test-dir build_qt5 -R node_editor_ide 2>&1 | tail -30
```

**Step 2: Verify existing v2 tests still pass**
Expect: TestLayoutTree, TestLayoutEngine, TestFlowFixtures (including video4viewsGridFixtureBuildsItsDeclaredGrid) green.

**Step 3: Manual smoke (minimal)**
- Create simple flow in NodeEditor, enter Layout Designer (F6), create grid 2x2, assign 4 nodes, add dock, Save Layout, save flow, reload. Verify v2 ui written.
- Run flow with NodeRunner --run and verify v2 layout builds (Task 11).

**Step 4: Commit (no code changes)**
```bash
cd /mnt/Builder/Projects/samiavasil/daqster
git status --short
```
(skip if clean)

