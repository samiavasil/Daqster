cmake_minimum_required(VERSION 3.16)

project(node_editor_ide VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# ══════════════════════════════════════════════════════════════════════
# Shared Library: NodeEditorLibrary
# Extracted so both NodeEditorIde and DemoNodeEditorNodes plugins
# can link against it without cross-plugin symbol resolution issues.
# Conditional on required Qt modules being available.
# ══════════════════════════════════════════════════════════════════════

register_component(NodeEditorLibrary
    REQUIRES_LIBRARIES
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        Qt${QT_VERSION_MAJOR}::Multimedia
        Qt${QT_VERSION_MAJOR}::Charts
        QtNodes
        frame_work
)

get_property(NODE_EDITOR_LIB_ENABLED GLOBAL PROPERTY COMPONENT_NodeEditorLibrary_ENABLED)

if(NODE_EDITOR_LIB_ENABLED)
    set(LIB_SOURCES
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
        BuiltInNodes/Library/threading/EventThreadPull.h
        BuiltInNodes/Library/threading/EventThreadPull.cpp
    )

    add_library(NodeEditorLibrary SHARED ${LIB_SOURCES})

    target_include_directories(NodeEditorLibrary PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/BuiltInNodes/Library/types
        ${CMAKE_CURRENT_SOURCE_DIR}/BuiltInNodes/Library/connectors
        ${CMAKE_CURRENT_SOURCE_DIR}/BuiltInNodes/Library/decoders
        ${CMAKE_CURRENT_SOURCE_DIR}/BuiltInNodes/Library/display
        ${CMAKE_CURRENT_SOURCE_DIR}/BuiltInNodes/Library/threading
        ${DAQSTER_SOURCE_DIR}/src/plugins/common
        ${DAQSTER_SOURCE_DIR}/src/frame_work/base/src/include
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

# ══════════════════════════════════════════════════════════════════════
# Plugin: NodeEditorIde (IDE shell + built-in nodes)
# ══════════════════════════════════════════════════════════════════════

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
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        Qt${QT_VERSION_MAJOR}::Network
        Qt${QT_VERSION_MAJOR}::Multimedia
        Qt${QT_VERSION_MAJOR}::Charts
        Qt${QT_VERSION_MAJOR}::OpenGL
        QtNodes
        frame_work
    INCLUDE_DIRECTORIES
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${DAQSTER_SOURCE_DIR}/src/plugins/common
        ${CMAKE_CURRENT_SOURCE_DIR}/BuiltInNodes/Sources/NumberSource
        ${CMAKE_CURRENT_SOURCE_DIR}/BuiltInNodes/Displays/NumberDisplay
        ${CMAKE_CURRENT_SOURCE_DIR}/BuiltInNodes/Operators/Modulo
        ${CMAKE_CURRENT_SOURCE_DIR}/BuiltInNodes/Operators/ArithmeticLogic
    COMPILE_DEFINITIONS
        NODE_EDITOR_SHARED
)

if(TARGET NodeEditorIde)
    set_target_properties(NodeEditorIde PROPERTIES
        OUTPUT_NAME "NodeEditorPluginIde"
    )
endif()
