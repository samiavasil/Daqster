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
# Modulo, Arithmetic/Logic) live in the DemoNodeEditorNodesCore plugin and
# are exposed through INodeProvider. They used to be DUPLICATED here and
# registered by NodeEditorIdeObject *before* the core plugin — since
# NodeDelegateModelRegistry::registerModel is first-wins by model name, the
# core (widget-free) versions were shadowed and never instantiated.
# The NodeEditorBuiltInNodes library that carried those duplicates is removed.
# ════════════════════════════════════════════════════════════════════════


# ═════════════════════════════════════════════════════════════════════════
# Shared Library: NodeEditorLibrary
# Extracted so both NodeEditorIde and DemoNodeEditorNodes plugins
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
)

get_property(NODE_EDITOR_LIB_ENABLED GLOBAL PROPERTY COMPONENT_NodeEditorLibrary_ENABLED)

if(NODE_EDITOR_LIB_ENABLED)
    set(LIB_SOURCES
        FlowUiSection.h
        FlowUiSection.cpp
        # Editor canvas (REQ-SW-PL-053): RuntimeShell in FrameworkGuiPlugin
        # drives a NodeEditorWidget to obtain the registry, the scene and the
        # graph model, so these belong in the shared library rather than in the
        # NodeEditorIde plugin — otherwise FrameworkGuiPlugin would have to
        # resolve symbols out of another plugin.
        NodeEditorWidget.h
        NodeEditorWidget.cpp
        CustomDataFlowScene.h
        CustomDataFlowScene.cpp
        BuiltInNodes/Library/types/GenericNumericTypes.h
        BuiltInNodes/Library/types/GenericNumericTypes.cpp
        BuiltInNodes/Library/types/IStreamDecoderObsolete.h
        BuiltInNodes/Library/types/QDevIOStreamConfigObsolete.h
        BuiltInNodes/Library/types/StreamChannelObsolete.h
        BuiltInNodes/Library/types/ChatGraphModel.h
        BuiltInNodes/Library/types/ChatGraphModel.cpp
        BuiltInNodes/Library/connectors/NodeDataModelToQIODeviceConnectorObsolete.h
        BuiltInNodes/Library/connectors/NodeDataModelToQIODeviceConnectorObsolete.cpp
        BuiltInNodes/Library/connectors/GenericQDevIoConnectorObsolete.h
        BuiltInNodes/Library/connectors/GenericQDevIoConnectorObsolete.cpp
        BuiltInNodes/Library/decoders/AudioFrameDecoder.h
        BuiltInNodes/Library/decoders/AudioFrameDecoder.cpp
        BuiltInNodes/Library/display/QDevIoDisplayModelObsolete.h
        BuiltInNodes/Library/display/QDevIoDisplayModelObsolete.cpp
        BuiltInNodes/Library/display/QDevioDisplayModelUiObsolete.h
        BuiltInNodes/Library/display/QDevioDisplayModelUiObsolete.cpp
        BuiltInNodes/Library/display/QDevioDisplayModelUiObsolete.ui
        BuiltInNodes/Library/display/XYSeriesIODeviceObsolete.h
        BuiltInNodes/Library/display/XYSeriesIODeviceObsolete.cpp
        BuiltInNodes/Library/display/AudioCompat.h
        BuiltInNodes/Library/display/QtChartsCompat.h
        BuiltInNodes/Library/threading/EventThreadPullObsolete.h
        BuiltInNodes/Library/threading/EventThreadPullObsolete.cpp
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
    # (NodeEditorIde, DemoNodeEditorNodes) do NOT define this macro, so the
    # macro expands to __declspec(dllimport) there. No-op on Linux/macOS.
    target_compile_definitions(NodeEditorLibrary PRIVATE NODE_EDITOR_LIBRARY_BUILD)

    target_include_directories(NodeEditorLibrary PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/BuiltInNodes/Library/types
        ${CMAKE_CURRENT_SOURCE_DIR}/BuiltInNodes/Library/connectors
        ${CMAKE_CURRENT_SOURCE_DIR}/BuiltInNodes/Library/decoders
        ${CMAKE_CURRENT_SOURCE_DIR}/BuiltInNodes/Library/display
        ${CMAKE_CURRENT_SOURCE_DIR}/BuiltInNodes/Library/threading
        ${DAQSTER_SOURCE_DIR}/src/frame_work/base/src/include
        ${DAQSTER_SOURCE_DIR}/src/frame_work/base/src/logging
        ${DAQSTER_SOURCE_DIR}/src/frame_work/base/src/gui
        ${DAQSTER_SOURCE_DIR}/src/plugins/common
        ${DAQSTER_SOURCE_DIR}/src/plugins/demo_nodeditor_nodes_core
        ${DAQSTER_SOURCE_DIR}/src/plugins/demo_nodeditor_nodes_gui
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

# REQ-SW-PL-051: the built-in models are owned by demo_nodeditor_nodes_core and
# reach this plugin through its INodeProvider. Nothing is compiled here: a
# second copy of the same class in another .so would break the dynamic_cast in
# DemoNodeEditorNodesGui::NodeWidgetFactory, which links the core plugin.

set(PLUGIN_RESOURCES
    node_editor.qrc
)

set(IDE_SOURCES
    NodeEditorIdeInterface.cpp
    NodeEditorIdeInterface.h
    NodeEditorIdeObject.cpp
    NodeEditorIdeObject.h
)

create_plugin(NodeEditorIde
    SOURCES
        ${IDE_SOURCES}
        ${PLUGIN_RESOURCES}
    REQUIRES_LIBRARIES
        NodeEditorLibrary
        QtNodes
      INCLUDE_DIRECTORIES
          ${DAQSTER_SOURCE_DIR}/src/plugins/demo_nodeditor_nodes_core/Displays/NumberDisplay
)

# ══════════════════════════════════════════════════════════════════════════
# Built-in nodes are now in NodeEditorBuiltInNodes library
# The plugin just uses them via NodeEditorLibrary
# ═══════════════════════════════════════════════════════════════════════════