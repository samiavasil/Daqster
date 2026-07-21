cmake_minimum_required(VERSION 3.16)

project(node_editor_ide VERSION 0.3.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# ══════════════════════════════════════════════════════════════════════
# Shared Library: NodeEditorLibrary
# Extracted so both NodeEditorIde and DemoNodeEditorNodes plugins
# can link against it without cross-plugin symbol resolution issues.
# ══════════════════════════════════════════════════════════════════════

set(LIB_SOURCES
    BuiltInNodes/Library/types/GenericNumericTypes.h
    BuiltInNodes/Library/types/GenericNumericTypes.cpp
    BuiltInNodes/Library/types/IStreamDecoder.h
    BuiltInNodes/Library/types/QDevIOStreamConfig.h
    BuiltInNodes/Library/types/StreamChannel.h
    BuiltInNodes/Library/types/NumericType.h
    BuiltInNodes/Library/types/ChatGraphModel.h
    BuiltInNodes/Library/connectors/NodeDataModelToQIODeviceConnector.h
    BuiltInNodes/Library/connectors/NodeDataModelToQIODeviceConnector.cpp
    BuiltInNodes/Library/connectors/GenericQDevIoConnector.h
    BuiltInNodes/Library/connectors/GenericQDevIoConnector.cpp
    BuiltInNodes/Library/decoders/AudioFrameDecoder.h
    BuiltInNodes/Library/decoders/AudioFrameDecoder.cpp
    BuiltInNodes/Library/display/QDevIoDisplayModel.h
    BuiltInNodes/Library/display/QDevIoDisplayModel.cpp
    BuiltInNodes/Library/display/QDevioDisplayModelUi.h
    BuiltInNodes/Library/display/QDevioDisplayModelUi.cpp
    BuiltInNodes/Library/display/QDevioDisplayModelUi.ui
    BuiltInNodes/Library/display/XYSeriesIODevice.h
    BuiltInNodes/Library/display/XYSeriesIODevice.cpp
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
    ${DAQSTER_SOURCE_DIR}/src/plugins/capabilities
)

target_link_libraries(NodeEditorLibrary PUBLIC
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Multimedia
    Qt${QT_VERSION_MAJOR}::Charts
    Qt${QT_VERSION_MAJOR}::OpenGL
    QtNodes
)

set_target_properties(NodeEditorLibrary PROPERTIES
    OUTPUT_NAME "NodeEditorLibrary"
)

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
        ${CMAKE_CURRENT_SOURCE_DIR}/BuiltInNodes/Sources/NumberSource
        ${CMAKE_CURRENT_SOURCE_DIR}/BuiltInNodes/Displays/NumberDisplay
        ${CMAKE_CURRENT_SOURCE_DIR}/BuiltInNodes/Operators/Modulo
        ${CMAKE_CURRENT_SOURCE_DIR}/BuiltInNodes/Operators/ArithmeticLogic
        ${DAQSTER_SOURCE_DIR}/src/plugins/capabilities
    COMPILE_DEFINITIONS
        NODE_EDITOR_SHARED
)

if(TARGET NodeEditorIde)
    set_target_properties(NodeEditorIde PROPERTIES
        OUTPUT_NAME "NodeEditorPluginIde"
    )
endif()
