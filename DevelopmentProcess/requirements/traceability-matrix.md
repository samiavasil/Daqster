# Traceability Matrix — Requirements Manager & Framework Tools

Матрица за проследимост на изискванията за **Requirements Manager** инструмент (`REQ-SW-*`).

## Изисквания (`REQ-SW-*`)

| REQ ID | Заглавие | Статус | Родител | Зависи от | Коммит(и) | Код | Тестове |
|--------|----------|--------|---------|-----------|-----------|-----|---------|
| `REQ-SW-001` | Requirements Viewer/Editor Tool (Base) | ACTIVE | — | — | — | `src/plugins/requirements_manager/` | Qt5/Qt6 builds |
| `REQ-SW-002` | Data Model & Parsing Extensions | ACTIVE | `REQ-SW-001` | `REQ-SW-001` | — | `src/plugins/requirements_manager/` | Qt5/Qt6 builds |
| `REQ-SW-003` | Requirement Creation Form UI | ACTIVE | `REQ-SW-001` | `REQ-SW-002` | — | `src/plugins/requirements_manager/` | Qt5/Qt6 builds |
| `REQ-SW-004` | Requirement Lifecycle & Status Management | ACTIVE | `REQ-SW-001` | `REQ-SW-002` | — | `src/plugins/requirements_manager/` | Qt5/Qt6 builds |
| `REQ-SW-005` | Built-in Help & Documentation Tab / Dialog | ACTIVE | `REQ-SW-001` | `REQ-SW-001` | — | `src/plugins/requirements_manager/` | Qt5/Qt6 builds |
| `REQ-SW-006` | Hierarchy Tree View | ACTIVE | `REQ-SW-001` | `REQ-SW-002` | — | `src/plugins/requirements_manager/` | Qt5/Qt6 builds |
| `REQ-SW-007` | Dependency Link Management & Navigation | ACTIVE | `REQ-SW-001` | `REQ-SW-002` | — | `src/plugins/requirements_manager/` | Qt5/Qt6 builds |
| `REQ-SW-008` | Requirements Validation & Consistency Engine | ACTIVE | `REQ-SW-001` | `REQ-SW-006`, `REQ-SW-007` | — | `src/plugins/requirements_manager/` | Qt5/Qt6 builds |
| `REQ-SW-009` | Interactive Dependency Graph Viewer | DONE | `REQ-SW-001` | `REQ-SW-007`, `REQ-SW-008` | `cf94e9e` (feat), `17ed818` (fix: edges follow nodes + viewport fit) | `src/plugins/requirements_manager/` | Qt5/Qt6 builds + `test_graph.*` (34 tests) + `test_graph_widget.*` (GUI, 4/4 green) |
| `REQ-SW-010` | Traceability Matrix View & Export | DONE | `REQ-SW-001` | `REQ-SW-002`, `REQ-SW-008` | `a26b603` (feat) | `src/plugins/requirements_manager/` | Qt5/Qt6 builds + `test_matrix.cpp` (7/7) + `test_exporter.cpp` (7/7) |
| `REQ-SW-013` | Plugin Manager Core | DONE | — | `REQ-SW-014` | `032e357`, `5731808`, `be51f46`, `61d93f5` | `src/frame_work/base/src/QPluginManager.cpp`, `QBasePluginObject.cpp`, `QPluginLoaderExt`, `registry/PluginRegistry.cpp` | Qt5/Qt6 builds |
| `REQ-SW-014` | Plugin Discovery, Persistence & Registry | DONE | — | — | `6910f2c`, `032e357`, `04c57f4` | `src/frame_work/base/src/discovery/`, `persistence/`, `registry/` | Qt5/Qt6 builds |
| `REQ-SW-015` | Plugin GUI & Debug Console | DONE | — | `REQ-SW-013` | `43c59a2`, `5eed6d6`, `db78014` | `src/frame_work/base/src/gui/` (QPluginManagerGui, QPluginListView, DebugConsoleWidget) | Qt5/Qt6 builds |
| `REQ-SW-016` | Platform Shutdown Handler | DONE | — | — | `0907d06`, `a1bce4b`, `027dc0a` | `src/frame_work/base/src/platform/ShutdownHandler*` | Qt5/Qt6 builds |
| `REQ-SW-017` | Logging Infrastructure | DONE | — | — | `5eed6d6`, `bc18fa4` | `src/frame_work/base/src/LogManager.cpp` | Qt5/Qt6 builds |
| `REQ-SW-018` | Process Management | DONE | — | `REQ-SW-017` | `5eed6d6`, `bc18fa4` | `src/frame_work/base/src/process/QProcessManager.*` | Qt5/Qt6 builds |
| `REQ-SW-019` | Daqster Application Host | DONE | — | `REQ-SW-013`, `REQ-SW-016`, `REQ-SW-017` | `0907d06`, `9ea787c`, `6b23593` | `src/apps/Daqster/` (main.cpp, ApplicationsManager, AppToolbar, AppSelectionDialog) | Qt5/Qt6 builds + headless smoke (offscreen) |
| `REQ-SW-020` | Shared Node API (Capabilities & NodeDataTypes) | DONE | — | — | `b16c409`, `e6edd62` | `src/plugins/common/capabilities/INodeProvider.h`, `src/plugins/common/NodeDataTypes/` | Qt5/Qt6 builds |
| `REQ-SW-021` | Node Editor IDE & Demo Nodes | DONE | — | `REQ-SW-020` | `f7aa532`, `fe87d14`, `e0f1395` | `src/plugins/node_editor_ide/`, `src/plugins/demo_nodeditor_nodes/` | Qt5/Qt6 builds |
| `REQ-SW-022` | QtCoinTrader Demo Plugin | DONE | — | `REQ-SW-013` | `5eed6d6`, `43c59a2`, `f0c0420` | `src/plugins/QtCoinTrader/` (QtCoinTraderPluginObject, About.qml, RequestForm, RestApiTester) | Qt5/Qt6 builds + QML smoke (Qt6) |
| `REQ-SW-023` | CMake Plugin Build Infrastructure | DONE | — | — | `e0f1395`, `1fec530`, `4cede60` | `cmake/ComponentTemplates.cmake`, `PluginDependencyManager.cmake`, `FindQtVersion.cmake` | Qt5/Qt6 builds на всички плъгини |
| `REQ-SW-024` | Unit Test Infrastructure | DONE | — | `REQ-SW-023` | `4825bfe`, `a26b603`, `17ed818` | `src/plugins/tests/`, `tests/plugins/requirements_manager/` | Qt5/Qt6 builds, 53 теста (requirements manager suite) |
| `REQ-SW-011` | Requirements Search Engine (Backlog) | BACKLOG | `REQ-SW-001` | `REQ-SW-002` | — | `src/plugins/requirements_manager/` | — |
| `REQ-SW-012` | Multi-Repository Requirements View (Merge) | BACKLOG | `REQ-SW-001` | `REQ-SW-002`, `REQ-SW-006`, `REQ-SW-010` | — | `src/plugins/requirements_manager/` | — |

## Plugin Framework (за справка — пълен trace в DaqsterAiStudio)

| REQ ID | Заглавие | Статус | Коммит(и) |
|--------|----------|--------|-----------|
| REQ-PLG-001 | AppImage & Search Path Normalization | DONE | `9ea787c`, `2412e56`, `04c57f4` |
| REQ-PLG-002 | Robust Plugin Discovery (non-"plugin" naming) | DONE | `032e357` |
| REQ-PLG-003 | Plugin Deduplication & Stale Persistency Pruning | DONE | `032e357` |
