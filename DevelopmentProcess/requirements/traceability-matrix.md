# Traceability Matrix — Requirements Manager & Framework Tools

Матрица за проследимост на изискванията за **Requirements Manager** инструмент (`REQ-SW-<TYPE>-*`).

## Изисквания (`REQ-SW-<TYPE>-*`)

| REQ ID | Заглавие | Статус | Родител | Зависи от | Коммит(и) | Код | Тестове |
|--------|----------|--------|---------|-----------|-----------|-----|---------|
| `REQ-SW-PL-001` | Requirements Viewer/Editor Tool (Base) | ACTIVE | — | — | — | `src/plugins/requirements_manager/` | Qt5/Qt6 builds |
| `REQ-SW-PL-002` | Data Model & Parsing Extensions | ACTIVE | `REQ-SW-PL-001` | `REQ-SW-PL-001` | — | `src/plugins/requirements_manager/` | Qt5/Qt6 builds |
| `REQ-SW-PL-003` | Requirement Creation Form UI | ACTIVE | `REQ-SW-PL-001` | `REQ-SW-PL-002` | — | `src/plugins/requirements_manager/` | Qt5/Qt6 builds |
| `REQ-SW-PL-004` | Requirement Lifecycle & Status Management | ACTIVE | `REQ-SW-PL-001` | `REQ-SW-PL-002` | — | `src/plugins/requirements_manager/` | Qt5/Qt6 builds |
| `REQ-SW-PL-005` | Built-in Help & Documentation Tab / Dialog | ACTIVE | `REQ-SW-PL-001` | `REQ-SW-PL-001` | — | `src/plugins/requirements_manager/` | Qt5/Qt6 builds |
| `REQ-SW-PL-006` | Hierarchy Tree View | ACTIVE | `REQ-SW-PL-001` | `REQ-SW-PL-002` | — | `src/plugins/requirements_manager/` | Qt5/Qt6 builds |
| `REQ-SW-PL-007` | Dependency Link Management & Navigation | ACTIVE | `REQ-SW-PL-001` | `REQ-SW-PL-002` | — | `src/plugins/requirements_manager/` | Qt5/Qt6 builds |
| `REQ-SW-PL-008` | Requirements Validation & Consistency Engine | ACTIVE | `REQ-SW-PL-001` | `REQ-SW-PL-006`, `REQ-SW-PL-007` | — | `src/plugins/requirements_manager/` | Qt5/Qt6 builds |
| `REQ-SW-PL-009` | Interactive Dependency Graph Viewer | DONE | `REQ-SW-PL-001` | `REQ-SW-PL-007`, `REQ-SW-PL-008` | `cf94e9e` (feat), `17ed818` (fix: edges follow nodes + viewport fit) | `src/plugins/requirements_manager/` | Qt5/Qt6 builds + `test_graph.*` (34 tests) + `test_graph_widget.*` (GUI, 4/4 green) |
| `REQ-SW-PL-010` | Traceability Matrix View & Export | DONE | `REQ-SW-PL-001` | `REQ-SW-PL-002`, `REQ-SW-PL-008` | `a26b603` (feat) | `src/plugins/requirements_manager/` | Qt5/Qt6 builds + `test_matrix.cpp` (7/7) + `test_exporter.cpp` (7/7) |
| `REQ-SW-PL-016` | Auto-Layout for Dependency Graph (Sugiyama) | DONE | `REQ-SW-PL-009` | `REQ-SW-PL-009` | *(feat, 2026-08-03)* | `src/plugins/requirements_manager/DependencyGraphLayout.{h,cpp}`, `DependencyGraphData.{h,cpp}`, `DependencyGraphWidget.cpp` | Qt5/Qt6 builds + `test_graph_layout.*` (6/6) — 68/68 PASS suite |
| `REQ-SW-PL-017` | Phase Status Indicators in the Requirements Main View | ACTIVE (impl done, unit tests deferred by user decision) | `REQ-SW-PL-001` | `REQ-SW-PL-002` | `4af3852` | `src/plugins/requirements_manager/RequirementsParser.{h,cpp}`, `RequirementsWidget.{h,cpp}` | Qt5/Qt6 builds + offscreen smoke (app loads, Daqster window present); unit tests (docs field + phase-status) deferred by user decision |
| `REQ-SW-FW-001` | Plugin Manager Core | DONE | — | `REQ-SW-FW-002` | `032e357`, `5731808`, `be51f46`, `61d93f5` | `src/frame_work/base/src/QPluginManager.cpp`, `QBasePluginObject.cpp`, `QPluginLoaderExt`, `registry/PluginRegistry.cpp` | Qt5/Qt6 builds |
| `REQ-SW-FW-002` | Plugin Discovery, Persistence & Registry | DONE | — | — | `6910f2c`, `032e357`, `04c57f4` | `src/frame_work/base/src/discovery/`, `persistence/`, `registry/` | Qt5/Qt6 builds |
| `REQ-SW-FW-003` | Plugin GUI & Debug Console | DONE | — | `REQ-SW-FW-001` | `43c59a2`, `5eed6d6`, `db78014` | `src/frame_work/base/src/gui/` (QPluginManagerGui, QPluginListView, DebugConsoleWidget) | Qt5/Qt6 builds |
| `REQ-SW-FW-004` | Platform Shutdown Handler | DONE | — | — | `0907d06`, `a1bce4b`, `027dc0a` | `src/frame_work/base/src/platform/ShutdownHandler*` | Qt5/Qt6 builds |
| `REQ-SW-FW-005` | Logging Infrastructure | DONE | — | — | `5eed6d6`, `bc18fa4` | `src/frame_work/base/src/LogManager.cpp` | Qt5/Qt6 builds |
| `REQ-SW-FW-006` | Process Management | DONE | — | `REQ-SW-FW-005` | `5eed6d6`, `bc18fa4` | `src/frame_work/base/src/process/QProcessManager.*` | Qt5/Qt6 builds |
| `REQ-SW-FW-007` | Plugin Security & Vendor Trust Store | ACTIVE (roadmap) | — | `REQ-SW-FW-001`, `REQ-SW-FW-002` | — | `src/frame_work/base/src/` (QPluginManager load path + VendorTrustStore) | — (roadmap) |
| `REQ-SW-APP-001` | Daqster Application Host | DONE | — | `REQ-SW-FW-001`, `REQ-SW-FW-004`, `REQ-SW-FW-005` | `0907d06`, `9ea787c`, `6b23593` | `src/apps/Daqster/` (main.cpp, ApplicationsManager, AppToolbar, AppSelectionDialog) | Qt5/Qt6 builds + headless smoke (offscreen) |
| `REQ-SW-PL-013` | Shared Node API (Capabilities & NodeDataTypes) | DONE | — | — | `b16c409`, `e6edd62` | `src/plugins/common/capabilities/INodeProvider.h`, `src/plugins/common/NodeDataTypes/` | Qt5/Qt6 builds |
| `REQ-SW-PL-014` | Node Editor IDE & Demo Nodes | DONE | — | `REQ-SW-PL-013` | `f7aa532`, `fe87d14`, `e0f1395` | `src/plugins/node_editor_ide/`, `src/plugins/demo_nodeditor_nodes/` | Qt5/Qt6 builds |
| `REQ-SW-PL-015` | QtCoinTrader Demo Plugin | DONE | — | `REQ-SW-FW-001` | `5eed6d6`, `43c59a2`, `f0c0420` | `src/plugins/QtCoinTrader/` (QtCoinTraderPluginObject, About.qml, RequestForm, RestApiTester) | Qt5/Qt6 builds + QML smoke (Qt6) |
| `REQ-SW-BLD-001` | CMake Plugin Build Infrastructure | DONE | — | — | `e0f1395`, `1fec530`, `4cede60` | `cmake/ComponentTemplates.cmake`, `PluginDependencyManager.cmake`, `FindQtVersion.cmake` | Qt5/Qt6 builds на всички плъгини |
| `REQ-SW-BLD-002` | Unit Test Infrastructure | DONE | — | `REQ-SW-BLD-001` | `4825bfe`, `a26b603`, `17ed818` | `src/plugins/tests/`, `tests/plugins/requirements_manager/` | Qt5/Qt6 builds, 53 теста (requirements manager suite) |
| `REQ-SW-PL-011` | Requirements Search Engine | ACTIVE (impl done, unit tests deferred by user decision) | `REQ-SW-PL-001` | `REQ-SW-PL-002` | `21d4315` | `src/plugins/requirements_manager/` (RequirementsSearchEngine, RequirementsWidget, HelpDialog) | Qt5/Qt6 builds + offscreen smoke (search narrows tree/graph/matrix; `status:DONE`; repo filter + search combined) |
| `REQ-SW-PL-012` | Multi-Repository Requirements View (Merge) | ACTIVE (impl done, unit tests deferred by user decision) | `REQ-SW-PL-001` | `REQ-SW-PL-002`, `REQ-SW-PL-006`, `REQ-SW-PL-010` | `c3d4e5c` | `src/plugins/requirements_manager/` (RequirementsParser, RequirementsWidget, RequirementsValidator, RequirementsModel, DependencyGraphData, TraceabilityMatrixModel, MatrixExporter) | Qt5/Qt6 builds + offscreen smoke (35 merged requirements, both roots) |
## Plugin Framework (за справка — пълен trace в DaqsterAiStudio)

| REQ ID | Заглавие | Статус | Коммит(и) |
|--------|----------|--------|-----------|
| REQ-PLG-001 | AppImage & Search Path Normalization | DONE | `9ea787c`, `2412e56`, `04c57f4` |
| REQ-PLG-002 | Robust Plugin Discovery (non-"plugin" naming) | DONE | `032e357` |
| REQ-PLG-003 | Plugin Deduplication & Stale Persistency Pruning | DONE | `032e357` |
