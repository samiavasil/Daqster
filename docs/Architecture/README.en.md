# Daqster Architecture

[Български](./README.md) | [English](./README.en.md)

Parent: [Documentation Index](../index.en.md)

## Architecture Hub

- [BuildSystemArchitecture.en.md](./BuildSystemArchitecture.en.md) - build architecture and dependency model
- [core-gui-split.md](./core-gui-split.md) - core (model) / gui (widgets) split
- [node_editor/README.md](./node_editor/README.md) - the node editor subsystem (QtNodes, IDE, nodes, ADRs)
- [rest-api-remote-control.md](./rest-api-remote-control.md) - REST + WebSocket control plane
- [capabilities.md](./capabilities.md) - capability interfaces (host ↔ plugin contracts)
- [runtime-mode-architecture.md](./runtime-mode-architecture.md) - (historical) runtime modes
- [apps/README.md](./apps/README.md) - applications subsystem
- [framework/README.md](./framework/README.md) - framework subsystem
- [plugins/README.md](./plugins/README.md) - plugins subsystem

## Related Sections

- [Development Topics](../development/README.md) - contributor workflow and debugging topics

This document describes the architecture of Daqster, its main components, data flow, and working principles.

## 1. Overview

Daqster is a modular, plugin-based platform developed with Qt (Qt6 is the primary/preferred version, Qt5 is compat), designed for data acquisition and analysis applications. It provides a flexible framework that allows easy extension through dynamically loaded plugins.

## 2. Main Architectural Components

### 2.1. Frame Work (Core)
This is the heart of Daqster, responsible for plugin management.

*   **`QPluginManager`**:
    *   Discovers and loads plugins from various directories (build, user, system, environment variables).
    *   Manages the lifecycle of plugins (loading, initialization, cleanup).
    *   Uses hash-based deduplication to ensure each plugin is loaded only once.
    *   Provides API for the host application to interact with plugins.
*   **`QPluginInterface`**:
    *   Abstract base class that all plugins must implement.
    *   Defines a standard interface for communication between the host application and plugins.
    *   Includes methods for initialization, starting, stopping, and accessing plugin metadata.
*   **`PluginFilter`**:
    *   Mechanism for filtering plugins based on their type (`APPLICATION_PLUGIN`, `DETECT_BY_TYPE_NAME`) or other metadata.
    *   Allows the host application to load only specific types of plugins.

### 2.2. Host Application (`src/apps/Daqster`)
The main application that provides user interface and manages plugin launching.

*   **`main.cpp`**:
    *   Application entry point.
    *   Initializes `QApplication` and `QPluginManager`.
    *   Handles command-line arguments for direct plugin launching.
    *   Detects whether the application is running as an AppImage and adapts its behavior.
*   **`ApplicationsManager`**:
    *   Responsible for launching plugins as separate processes (`QProcess`).
    *   Properly configures environment variables (`LD_LIBRARY_PATH`, `QT_PLUGIN_PATH`, `DAQSTER_PLUGIN_DIR`, etc.) for each child process, considering whether it's running in AppImage or as a regular build.
    *   Ensures that plugins can find their dependencies and resources.
*   **`AppToolbar`**:
    *   Manages GUI elements such as menus and buttons for launching plugins.
    *   Sends requests to `ApplicationsManager` to launch selected plugins.

> **`NodeRunner`** (`src/apps/NodeRunner`) is a second host — the same binary runs a
> `.flow` scene in GUI mode or with `--headless`. Both hosts share the widget-free
> `Daqster::AppBootstrap` (REQ-SW-PL-048). See [apps/NodeRunner.md](./apps/NodeRunner.md).

### 2.3. Plugin System (`src/plugins`)
Plugins that add functionality to the Daqster host.

*   **`node_editor/`** (group — everything around the node editor):
    *   **`node_editor_ide/`**: `APPLICATION_PLUGIN` IDE shell — `NodeEditorIde` + `NodeEditorWidget` (QtNodes canvas), discovers external `INodeProvider` plugins; contains the shared `NodeSupport` library (builds `NodeEditorLibrary`).
    *   **`nodeditor_nodes/{core,gui}/`**: the main node provider — `core` is headless (`INodeProvider`, no widgets), `gui` provides the widgets (`IWidgetProvider`).
*   **`runtime_host/{headless,gui}/`**:
    *   Runtime host plugins (`IRuntimeHost`) that bring up a `.flow` scene — headless (`HeadlessEngine`) or GUI (`RuntimeShell` + MDI).
*   **`QtCoinTrader`**:
    *   Plugin for cryptocurrency trading, using QML for user interface.
    *   Demonstrates QML and QtCharts integration.
    *   Can work as a standalone application.
*   **Test Plugins (e.g., `plugin_fancy_test`, `plugin_uggly_test`, `template_plugin_daqster`)**:
    *   Simple plugins used for testing `QPluginManager` and `QPluginInterface` functionality.
    *   Show how to implement `QPluginInterface`.

> Plugins come in `APPLICATION_PLUGIN`, Node Provider, Widget Provider and Runtime Host
> types; the contracts live in [capabilities.md](./capabilities.md).

### 2.4. External Libraries (`src/plugins/external_libs/`)
External libraries integrated as git submodules under the plugins directory.

*   **`nodeeditor`**:
    *   Library for creating graphical node editors.
    *   Produces the `QtNodesCore` (model-only) + `QtNodes` (widgets) targets (pinned at commit `4709573`).
    *   Used by `NodeEditorWidget`, `NodeEditorIde` and the node models.
*   **`qtrest_lib`**:
    *   Library for REST API communication.
    *   Can be used by plugins that require HTTP requests.

### 2.5. NodeEditorWidget & NodeEditorLibrary (Shared GUI Components)
**Location:** `src/plugins/node_editor/node_editor_ide/` — `NodeEditorWidget.{h,cpp}` and `NodeSupport/` (builds the `NodeEditorLibrary` shared library)

Shared Qt Widgets GUI components for node-based editors, built and installed from the `node_editor_ide` plugin directory. Any Daqster-based GUI node application should use these components instead of building GUI from scratch.

*   **`NodeEditorWidget`**:
    *   Main QWidget with `getInjectedRegistry()`, `buildCanvas()`, `setConnectionStyle()`.
    *   Auto-injects all standard Daqster open-source nodes (Audio, Media, Graphs, AI, etc.).
    *   Emits `nodeDoubleClicked` signal on node double-click.
*   **`ChatGraphModel`**:
    *   Loop-enabled graph model with circular link support.

### 2.6. Tools (`tools/`)
Directory containing scripts and helper files for build and packaging.

*   **`create_appimage.sh`**:
    *   Universal shell script for creating AppImage.
    *   Supports `local` and `ci` modes with different configurations.
    *   Automates the process of copying files, setting environment variables, and packaging.
*   **`Build_AppImage/`**:
    *   Directory used for local AppImage builds. Contains temporary files and the final AppImage.

### 2.6. Docs (`docs/`)
Directory for documentation.

*   **`development/HowToDebugAppImage.md`**:
    *   Guide for debugging AppImage applications.
*   **`development/DeveloperGuide.md`**:
    *   Contributor workflow and development notes.
*   **`operations/BuildHelpers.en.md`**:
    *   Helper scripts for build and maintenance tasks.
*   **`operations/UpstreamManagement.md`**:
    *   Guide for synchronizing and maintaining upstream dependencies.
*   **`porting/QtRest_Qt6_Porting.md`**:
    *   Notes and decisions related to Qt6 porting of QtRest.
*   **`Architecture/README.en.md`**:
    *   This document.

## 3. Key Architectural Principles

### 1. Modularity and Extensibility
- The core is separated from plugins, allowing easy addition of new functionality without changing the main code.
- Plugins are independent and can be developed and tested separately.

### 2. Dynamic Loading
- Plugins are loaded at runtime, which optimizes application startup time and provides flexibility.

### 3. Process Isolation
- Each plugin is launched as a separate child process, which increases stability and security. A problem in one plugin does not lead to a crash of the entire application.

### 4. Cross-platform
- Qt6 for GUI and cross-platform compatibility (Qt5 as compat fallback)
- CMake for build system
- AppImage for Linux distribution

## Data Flow

### 1. Application Startup
```
main.cpp
├── Initialize QApplication
├── Create QPluginManager
├── Load plugins
└── Start GUI
```

### 2. Plugin Loading
```
QPluginManager
├── Scan directories for plugins
├── Load .so files
├── Initialize QPluginInterface
└── Add to active plugins list
```

### 3. Plugin Launch
```
AppToolbar/ApplicationsManager
├── Receive launch request
├── Find plugin by name
├── Create QProcess with environment
└── Launch as child process
```

## Architecture Diagram

![Architecture Diagram](../diagrams/architecture.svg)

[PlantUML source](../diagrams/architecture.puml)

## Plugin Discovery Flow

![Plugin Discovery Flow](../diagrams/startup_sequence.svg)

[PlantUML source](../diagrams/startup_sequence.puml)

## 5. Environment Variables

### Plugin Discovery
- `DAQSTER_PLUGIN_DIR`: Sets a single directory for searching Daqster plugins.
- `DAQSTER_PLUGIN_PATH`: Sets multiple directories for searching Daqster plugins, separated by `:`.

### Qt Environment (in AppImage)
- `LD_LIBRARY_PATH`: Tells the dynamic linker where to look for shared libraries.
- `QML2_IMPORT_PATH`: Tells the QML engine where to look for QML modules.
- `QT_PLUGIN_PATH`: Tells Qt where to look for its plugins (e.g., image formats, SQL drivers).
- `QT_QPA_PLATFORM_PLUGIN_PATH`: Specifies the path to platform integration plugins (e.g., `libqxcb.so`).

### XDG Directories (for AppImage)
- `XDG_CONFIG_HOME`: Directory for user configuration files (default: `~/.config/daqster`).
- `XDG_DATA_HOME`: Directory for user data (default: `~/.local/share/daqster`).
- `XDG_CACHE_HOME`: Directory for user cache files (default: `~/.cache/daqster`).

### Debug Environment
- `QT_DEBUG_PLUGINS=1`: Enables detailed debug information for Qt plugin loading.
- `QT_LOGGING_RULES="*=true"`: Enables all Qt debug messages.
- `DAQSTER_DEBUG=1`: Enables Daqster-specific debug messages (if implemented).

## 6. Security Considerations

- **Plugin Isolation:** Launching plugins as separate processes provides some isolation, but it's not a complete sandbox environment.
- **Code Signing:** For production environments, consider signing plugins to verify their authenticity.
- **Input Validation:** All data coming from plugins or external sources must be validated.

## 7. Performance Considerations

- **Dynamic Loading:** Plugins are loaded only when needed, which optimizes startup time.
- **Process Overhead:** Launching each plugin as a separate process has a small overhead, but provides stability.
- **Resource Management:** Efficient management of memory and resources in plugins is crucial.

## 8. Testing Strategy

- **Unit Tests:** For individual components and plugin logic.
- **Integration Tests:** For interaction between the host application and plugins.
- **AppImage Tests:** Automated tests for AppImage startup and functionality.
- **Manual Testing:** For GUI and user experience.

## 9. Plugin Lifecycle Diagram

![Plugin Lifecycle](../diagrams/plugin_lifecycle.svg)

[PlantUML source](../diagrams/plugin_lifecycle.puml)

## 10. AppImage Structure Diagram

```text
Daqster-x86_64.AppImage
|-- AppRun                          <- startup script
|-- daqster.desktop
|-- daqster.png
`-- usr/
    |-- bin/
    |   `-- Daqster                 <- main executable
    |-- lib/
    |   |-- libQt*.so.*             <- Qt libraries
    |   |-- libicu*.so.*            <- ICU libraries
    |   |-- plugins/                <- Qt plugins
    |   |-- qml/                    <- QML modules
    |   `-- daqster/
    |       `-- plugins/            <- Daqster plugins
    `-- share/
        |-- applications/           <- desktop file
        `-- icons/                  <- app icon
```

## 11. Future Enhancements

- **Plugin Management GUI:** Integrated user interface for managing installed plugins (enable/disable, install/uninstall).
- **Sandbox Environment:** Stricter isolation of plugins (e.g., through Flatpak or Docker).
- **Remote Plugin Loading:** Ability to load plugins from remote sources.
- **Performance Monitoring:** Tools for monitoring plugin performance.
- **Cross-platform AppImages:** Extending AppImage builds for other architectures.