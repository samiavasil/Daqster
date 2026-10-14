cmake_minimum_required(VERSION 3.16)

project(node_editor_ide VERSION 0.3.2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# ── Plugin JSON metadata (generated from template) ──────────────────
# REQ-SW-PL-035: the plugin metadata JSON is generated from a .json.in
# template with @DAQSTER_VERSION@ substituted (single source of truth:
# root VERSION file). Must run BEFORE create_plugin() so the JSON exists
# when moc resolves Q_PLUGIN_METADATA FILE.
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/NodeEditorIdeInterface.json.in"
    "${CMAKE_CURRENT_SOURCE_DIR}/NodeEditorIdeInterface.json"
    @ONLY
)

# ════════════════════════════════════════════════════════════════════════
# REQ-SW-PL-051: the built-in node models (NumberSource, NumberDisplay,
# Modulo, Arithmetic/Logic) live in the NodeditorNodesCore plugin and
# are exposed through INodeProvider. They used to be DUPLICATED here and
# registered by NodeEditorIdeObject *before* the core plugin — since
# NodeDelegateModelRegistry::registerModel is first-wins by model name, the
# core (widget-free) versions were shadowed and never instantiated.
# The NodeEditorBuiltInNodes library that carried those duplicates is removed.
# ════════════════════════════════════════════════════════════════════════


# ═════════════════════════════════════════════════════════════════════════
# Shared Library: NodeEditorLibrary
# Extracted so both NodeEditorIde and NodeditorNodes plugins
# can link against it without cross-plugin symbol resolution issues.
# Conditional on required Qt modules being available.
# ═════════════════════════════════════════════════════════════════════════

register_component(NodeEditorLibrary
    REQUIRES_LIBRARIES
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        Qt${QT_VERSION_MAJOR}::Multimedia
        Qt${QT_VERSION_MAJOR}::Charts
        QtNodes
        # NodeEditorWidget.cpp, EventThreadPullObsolete.cpp,
        # XYSeriesIODeviceObsolete.cpp and QDevioDisplayModelObsolete.cpp all
        # log through lcNodeEditor(),
        # which is defined in base/src/logging/LogCategories.cpp (part of
        # FrameworkCore) and declared FRAMEWORK_CORE_EXPORT. Without this,
        # linking NodeEditorLibrary.dll fails with:
        #   error LNK2001: unresolved external symbol
        #   "__declspec(dllimport) ... lcNodeEditor(void)"
        #   fatal error LNK1120: 1 unresolved externals
        FrameworkCore
)

get_property(NODE_EDITOR_LIB_ENABLED GLOBAL PROPERTY COMPONENT_NodeEditorLibrary_ENABLED)

if(NODE_EDITOR_LIB_ENABLED)
    set(LIB_SOURCES
        FlowUiSection.h
        FlowUiSection.cpp
        # Schema-v2 layout model (LayoutTree, QtCore-only) + widget builder
        # (LayoutEngine, QtWidgets). Moved here from the NodeEditorIde plugin so
        # BOTH consumers can link them: NodeEditorIdeObject (Layout Designer,
        # REQ-SW-PL-059) and RuntimeShell (v2 runtime layout, REQ-SW-PL-053).
        LayoutTree.h
        LayoutTree.cpp
        LayoutEngine.h
        LayoutEngine.cpp
        # Editor canvas (REQ-SW-PL-053): RuntimeShell in RuntimeHostGuiPlugin
        # drives a NodeEditorWidget to obtain the registry, the scene and the
        # graph model, so these belong in the shared library rather than in the
        # NodeEditorIde plugin — otherwise RuntimeHostGuiPlugin would have to
        # resolve symbols out of another plugin.
        NodeEditorWidget.h
        NodeEditorWidget.cpp
        CustomDataFlowScene.h
        CustomDataFlowScene.cpp
        NodeSupport/types/GenericNumericTypes.h
        NodeSupport/types/GenericNumericTypes.cpp
        NodeSupport/types/IStreamDecoderObsolete.h
        NodeSupport/types/QDevIOStreamConfigObsolete.h
        NodeSupport/types/StreamChannelObsolete.h
        NodeSupport/types/ChatGraphModel.h
        NodeSupport/types/ChatGraphModel.cpp
        NodeSupport/connectors/NodeDataModelToQIODeviceConnectorObsolete.h
        NodeSupport/connectors/NodeDataModelToQIODeviceConnectorObsolete.cpp
        NodeSupport/connectors/GenericQDevIoConnectorObsolete.h
        NodeSupport/connectors/GenericQDevIoConnectorObsolete.cpp
        NodeSupport/decoders/AudioFrameDecoder.h
        NodeSupport/decoders/AudioFrameDecoder.cpp
        NodeSupport/display/QDevIoDisplayModelObsolete.h
        NodeSupport/display/QDevIoDisplayModelObsolete.cpp
        NodeSupport/display/XYSeriesIODeviceObsolete.h
        NodeSupport/display/XYSeriesIODeviceObsolete.cpp
        NodeSupport/display/AudioCompat.h
        NodeSupport/display/QtChartsCompat.h
        NodeSupport/threading/EventThreadPullObsolete.h
        NodeSupport/threading/EventThreadPullObsolete.cpp
    )

    add_library(NodeEditorLibrary SHARED ${LIB_SOURCES})

    # Windows: NodeEditorLibrary has no __declspec(dllexport) markup, so without
    # this the DLL exports nothing and no import library (.lib) is generated —
    # plugins linking against it fail with LNK1181. WINDOWS_EXPORT_ALL_SYMBOLS
    # makes the linker export all symbols and emit the import library. No-op on
    # Linux/macOS (.so/.dylib export everything by default).
    set_target_properties(NodeEditorLibrary PROPERTIES
        WINDOWS_EXPORT_ALL_SYMBOLS ON
    )

    # When building NodeEditorLibrary itself, Q_OBJECT classes marked with
    # NODE_EDITOR_LIBRARY_EXPORT expand to __declspec(dllexport) on Windows so
    # their staticMetaObject data symbols are exported from the DLL. Consumers
    # (NodeEditorIde, NodeditorNodes) do NOT define this macro, so the
    # macro expands to __declspec(dllimport) there. No-op on Linux/macOS.
    target_compile_definitions(NodeEditorLibrary PRIVATE NODE_EDITOR_LIBRARY_BUILD)

    target_include_directories(NodeEditorLibrary PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/NodeSupport/types
        ${CMAKE_CURRENT_SOURCE_DIR}/NodeSupport/connectors
        ${CMAKE_CURRENT_SOURCE_DIR}/NodeSupport/decoders
        ${CMAKE_CURRENT_SOURCE_DIR}/NodeSupport/display
        ${CMAKE_CURRENT_SOURCE_DIR}/NodeSupport/threading
        ${DAQSTER_SOURCE_DIR}/src/frame_work/base/src/include
        ${DAQSTER_SOURCE_DIR}/src/frame_work/base/src/logging
        ${DAQSTER_SOURCE_DIR}/src/frame_work/base/src/gui
        ${DAQSTER_SOURCE_DIR}/src/plugins/common
        ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/core
        ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/gui
        # framework_core_export.h is generated in the public daqster build tree
        ${CMAKE_BINARY_DIR}/src/frame_work
    )

    link_component_dependencies(NodeEditorLibrary)

    # QtOpenGL is optional — link if available
    if(TARGET Qt${QT_VERSION_MAJOR}::OpenGL)
        target_link_libraries(NodeEditorLibrary PUBLIC Qt${QT_VERSION_MAJOR}::OpenGL)
    endif()

    set_target_properties(NodeEditorLibrary PROPERTIES
        OUTPUT_NAME "NodeEditorLibrary"
    )

    # Install library (dependency for NodeEditorPluginIde)
    install(TARGETS NodeEditorLibrary
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )

    message(STATUS "Created internal library: NodeEditorLibrary")
else()
    get_property(REASONS GLOBAL PROPERTY COMPONENT_NodeEditorLibrary_REASONS)
    message(STATUS "Skipping library NodeEditorLibrary - dependencies not met:")
    foreach(REASON ${REASONS})
        message(STATUS "  - ${REASON}")
    endforeach()
endif()

# ══════════════════════════════════════════════════════════════════════════
# Plugin: NodeEditorIde (IDE shell + built-in nodes)
# ══════════════════════════════════════════════════════════════════════════

# REQ-SW-PL-051: the built-in models are owned by nodeditor_nodes/core and
# reach this plugin through its INodeProvider. Nothing is compiled here: a
# second copy of the same class in another .so would break the dynamic_cast in
# NodeditorNodesGui::NodeWidgetFactory, which links the core plugin.

set(PLUGIN_RESOURCES
    node_editor.qrc
)

set(IDE_SOURCES
    NodeEditorIdeInterface.cpp
    NodeEditorIdeInterface.h
    NodeEditorIdeObject.cpp
    NodeEditorIdeObject.h
    # Workspace layout engine (REQ-SW-PL-053 Phase 2a) now lives in the shared
    # NodeEditorLibrary so RuntimeShell can build v2 layouts too — see
    # LIB_SOURCES. This plugin links NodeEditorLibrary and gets the symbols.
    #
    # Visual layout designer (REQ-SW-PL-059): edits a v2 LayoutSection with the
    # mouse (structure tree + live LayoutEngine preview) instead of hand-editing
    # the .flow "ui" JSON.
    LayoutDesignerPanel.cpp
    LayoutDesignerPanel.h
)

create_plugin(NodeEditorIde
    SOURCES
        ${IDE_SOURCES}
        ${PLUGIN_RESOURCES}
    REQUIRES_LIBRARIES
        NodeEditorLibrary
        QtNodes
        # NodeEditorIdeObject.cpp opens the plugin manager dialog:
        #   new Daqster::QPluginManagerGui(m_Win)
        # QPluginManagerGui is declared FRAMEWORK_GUI_EXPORT and its
        # constructor/destructor/vtable live in FrameworkGui.dll. Without this,
        # linking the plugin fails on Windows with:
        #   error LNK2001: unresolved external symbol
        #   "__declspec(dllimport) ... QPluginManagerGui::QPluginManagerGui(QWidget*)"
        #   fatal error LNK1120: N unresolved externals
        # Linux tolerates it (undefined symbols resolve lazily at dlopen),
        # Windows does not. QtCoinTrader does the same thing and already
        # links FrameworkGui.
        FrameworkGui
      INCLUDE_DIRECTORIES
          ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/core/Displays/NumberDisplay
)

# ══════════════════════════════════════════════════════════════════════════
# Built-in nodes are now in NodeEditorBuiltInNodes library
# The plugin just uses them via NodeEditorLibrary
# ═══════════════════════════════════════════════════════════════════════════