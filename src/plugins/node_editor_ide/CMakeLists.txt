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
# Shared Library: NodeEditorBuiltInNodes
# Contains only the built-in node models (no GUI dependencies)
# This can be used by FrameworkCore (for headless) and NodeEditorLibrary
# ══════════════════════════════════════════════════════════════════════════

register_component(NodeEditorBuiltInNodes
    REQUIRES_LIBRARIES
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        Qt${QT_VERSION_MAJOR}::Widgets
        QtNodes
)

get_property(NODE_EDITOR_BUILTIN_NODES_ENABLED GLOBAL PROPERTY COMPONENT_NodeEditorBuiltInNodes_ENABLED)

if(NODE_EDITOR_BUILTIN_NODES_ENABLED)
    set(BUILTIN_NODES_SOURCES
        BuiltInNodes/Sources/NumberSource/NumberSourceDataModel.cpp
        BuiltInNodes/Sources/NumberSource/NumberSourceDataUi.cpp
        BuiltInNodes/Displays/NumberDisplay/NumberDisplayDataModel.cpp
        BuiltInNodes/Operators/Modulo/ModuloModel.cpp
        BuiltInNodes/Operators/ArithmeticLogic/ArithmeticLogicModel.cpp
        BuiltInNodes/Operators/ArithmeticLogic/ExprParser.cpp
    )

    add_library(NodeEditorBuiltInNodes SHARED ${BUILTIN_NODES_SOURCES})

    # Windows: export all symbols
    set_target_properties(NodeEditorBuiltInNodes PROPERTIES
        WINDOWS_EXPORT_ALL_SYMBOLS ON
    )

    target_compile_definitions(NodeEditorBuiltInNodes PRIVATE NODE_EDITOR_BUILTIN_NODES_BUILD)

    target_include_directories(NodeEditorBuiltInNodes PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/BuiltInNodes/Library/types
        ${DAQSTER_SOURCE_DIR}/src/frame_work/base/src/include
        ${DAQSTER_SOURCE_DIR}/src/frame_work/base/src/logging
        ${DAQSTER_SOURCE_DIR}/src/plugins/common
    )

    # Link Qt dependencies
    target_link_libraries(NodeEditorBuiltInNodes PUBLIC
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        QtNodes
    )

    set_target_properties(NodeEditorBuiltInNodes PROPERTIES
        OUTPUT_NAME "NodeEditorBuiltInNodes"
    )

    # Install library
    install(TARGETS NodeEditorBuiltInNodes
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )

    install(FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/BuiltInNodes/Sources/NumberSource/NumberSourceDataModel.h
        ${CMAKE_CURRENT_SOURCE_DIR}/BuiltInNodes/Sources/NumberSource/NumberSourceDataUi.h
        ${CMAKE_CURRENT_SOURCE_DIR}/BuiltInNodes/Displays/NumberDisplay/NumberDisplayDataModel.h
        ${CMAKE_CURRENT_SOURCE_DIR}/BuiltInNodes/Operators/Modulo/ModuloModel.h
        ${CMAKE_CURRENT_SOURCE_DIR}/BuiltInNodes/Operators/ArithmeticLogic/ArithmeticLogicModel.h
        ${CMAKE_CURRENT_SOURCE_DIR}/BuiltInNodes/Operators/ArithmeticLogic/ExprParser.h
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/NodeEditorBuiltInNodes
    )

    message(STATUS "Created library: NodeEditorBuiltInNodes (built-in node models)")
else()
    get_property(REASONS GLOBAL PROPERTY COMPONENT_NodeEditorBuiltInNodes_REASONS)
    message(STATUS "Skipping library NodeEditorBuiltInNodes - dependencies not met:")
    foreach(REASON ${REASONS})
        message(STATUS "  - ${REASON}")
    endforeach()
endif()

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
        FrameworkGui
        NodeEditorBuiltInNodes
)

get_property(NODE_EDITOR_LIB_ENABLED GLOBAL PROPERTY COMPONENT_NodeEditorLibrary_ENABLED)

if(NODE_EDITOR_LIB_ENABLED)
    set(LIB_SOURCES
        FlowUiSection.h
        FlowUiSection.cpp
        BuiltInNodes/Library/types/GenericNumericTypes.h
        BuiltInNodes/Library/types/GenericNumericTypes.cpp
        BuiltInNodes/Library/types/IStreamDecoderObsolete.h
        BuiltInNodes/Library/types/QDevIOStreamConfigObsolete.h
        BuiltInNodes/Library/types/StreamChannelObsolete.h
        BuiltInNodes/Library/types/ChatGraphModel.h
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
        ${CMAKE_CURRENT_SOURCE_DIR}/BuiltInNodes/Sources/NumberSource
        ${CMAKE_CURRENT_SOURCE_DIR}/BuiltInNodes/Displays/NumberDisplay
        ${CMAKE_CURRENT_SOURCE_DIR}/BuiltInNodes/Operators/Modulo
        ${CMAKE_CURRENT_SOURCE_DIR}/BuiltInNodes/Operators/ArithmeticLogic
        ${DAQSTER_SOURCE_DIR}/src/frame_work/base/src/include
        ${DAQSTER_SOURCE_DIR}/src/frame_work/base/src/logging
        ${DAQSTER_SOURCE_DIR}/src/frame_work/base/src/gui
        ${DAQSTER_SOURCE_DIR}/src/plugins/common
        ${DAQSTER_SOURCE_DIR}/src/plugins/demo_nodeditor_nodes_core
        # framework_core_export.h is generated in the public daqster build tree
        ${CMAKE_BINARY_DIR}/daqster/src/frame_work
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

set(BUILTIN_SOURCE_NODES
    BuiltInNodes/Sources/NumberSource/NumberSourceDataModel.h
    BuiltInNodes/Sources/NumberSource/NumberSourceDataModel.cpp
    BuiltInNodes/Sources/NumberSource/NumberSourceDataUi.h
    BuiltInNodes/Sources/NumberSource/NumberSourceDataUi.cpp
    BuiltInNodes/Sources/NumberSource/NumberSourceDataUi.ui
)

set(BUILTIN_DISPLAY_NODES
    BuiltInNodes/Displays/NumberDisplay/NumberDisplayDataModel.h
    BuiltInNodes/Displays/NumberDisplay/NumberDisplayDataModel.cpp
)

set(BUILTIN_OPERATOR_NODES
    BuiltInNodes/Operators/Modulo/ModuloModel.h
    BuiltInNodes/Operators/Modulo/ModuloModel.cpp
    BuiltInNodes/Operators/ArithmeticLogic/ExprParser.h
    BuiltInNodes/Operators/ArithmeticLogic/ExprParser.cpp
    BuiltInNodes/Operators/ArithmeticLogic/ArithmeticLogicModel.h
    BuiltInNodes/Operators/ArithmeticLogic/ArithmeticLogicModel.cpp
)

set(PLUGIN_RESOURCES
    node_editor.qrc
)

set(IDE_SOURCES
    NodeEditorIdeInterface.cpp
    NodeEditorIdeInterface.h
    NodeEditorIdeObject.cpp
    NodeEditorIdeObject.h
    NodeEditorWidget.cpp
    NodeEditorWidget.h
    CustomDataFlowScene.cpp
    CustomDataFlowScene.h
    RuntimeShell.cpp
    RuntimeShell.h
)

create_plugin(NodeEditorIde
    SOURCES
        ${IDE_SOURCES}
        ${BUILTIN_SOURCE_NODES}
        ${BUILTIN_DISPLAY_NODES}
        ${BUILTIN_OPERATOR_NODES}
        ${PLUGIN_RESOURCES}
    REQUIRES_LIBRARIES
        NodeEditorLibrary
        QtNodes
)

# ══════════════════════════════════════════════════════════════════════════
# Built-in nodes are now in NodeEditorBuiltInNodes library
# The plugin just uses them via NodeEditorLibrary
# ═══════════════════════════════════════════════════════════════════════════