cmake_minimum_required(VERSION 3.16)

project(node_editor_ide VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# ── Built-in node model sources (moved from node_editor_app) ─────────
set(BUILTIN_NODE_SOURCES
    BuiltInNodes/Audio/AudioNodeQdevIoConnector.cpp
    BuiltInNodes/Audio/AudioComboModel.cpp
    BuiltInNodes/Audio/AudioSourceConfig.cpp
    BuiltInNodes/Audio/AudioSourceDataModel.cpp
    BuiltInNodes/Audio/AudioSourceDataModelUI.cpp
    BuiltInNodes/Audio/AudioWorker.cpp
    BuiltInNodes/Library/NodeDataModelToQIODeviceConnector.cpp
    BuiltInNodes/Library/GenericQDevIoConnector.cpp
    BuiltInNodes/Library/GenericNumericTypes.cpp
    BuiltInNodes/Library/EventThreadPull.cpp
    BuiltInNodes/Examples/Converters.cpp
    BuiltInNodes/Library/QDevIoDisplayModel.cpp
    BuiltInNodes/Library/QDevioDisplayModelUi.cpp
    BuiltInNodes/Library/AudioFrameDecoder.cpp
    BuiltInNodes/Library/XYSeriesIODevice.cpp
    BuiltInNodes/LLama/LLamaModelDataModel.cpp
    BuiltInNodes/LLama/ConsoleDataModel.cpp
    BuiltInNodes/LLama/ChatBaseWidget.cpp
    BuiltInNodes/Audio/AudioNodeQdevIoConnector.h
    BuiltInNodes/Audio/AudioComboModel.h
    BuiltInNodes/Audio/AudioSourceConfig.h
    BuiltInNodes/Audio/AudioSourceDataModel.h
    BuiltInNodes/Audio/AudioSourceDataModelUI.h
    BuiltInNodes/Audio/AudioWorker.h
    BuiltInNodes/Library/NodeDataModelToQIODeviceConnector.h
    BuiltInNodes/Library/IStreamDecoder.h
    BuiltInNodes/Library/QDevIOStreamConfig.h
    BuiltInNodes/Library/StreamChannel.h
    BuiltInNodes/Library/GenericQDevIoConnector.h
    BuiltInNodes/Library/GenericNumericTypes.h
    BuiltInNodes/Library/EventThreadPull.h
    BuiltInNodes/Examples/Converters.h
    BuiltInNodes/Examples/DecimalData.h
    BuiltInNodes/Examples/IntegerData.h
    BuiltInNodes/Library/ComplexType.h
    BuiltInNodes/Library/AudioFrameDecoder.h
    BuiltInNodes/Library/QDevIoDisplayModel.h
    BuiltInNodes/Library/QDevioDisplayModelUi.h
    BuiltInNodes/Library/XYSeriesIODevice.h
    BuiltInNodes/LLama/TextData.h
    BuiltInNodes/LLama/LLamaModelDataModel.h
    BuiltInNodes/LLama/ConsoleDataModel.h
    BuiltInNodes/LLama/ChatBaseWidget.h
    BuiltInNodes/Audio/AudioSourceConfig.ui
    BuiltInNodes/Audio/AudioSourceDataModelUI.ui
    BuiltInNodes/Library/QDevioDisplayModelUi.ui
    BuiltInNodes/Audio/node_editor.qrc
)

# ── Plugin IDE sources ───────────────────────────────────────────────
set(IDE_SOURCES
    NodeEditorIdeInterface.cpp
    NodeEditorIdeInterface.h
    NodeEditorIdeObject.cpp
    NodeEditorIdeObject.h
    NodeEditorWidget.cpp
    NodeEditorWidget.h
    ChatGraphModel.h
)

# ── Use the Daqster plugin template ──────────────────────────────────
create_plugin(NodeEditorIde
    SOURCES
        ${IDE_SOURCES}
        ${BUILTIN_NODE_SOURCES}
    REQUIRES_LIBRARIES
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
        ${CMAKE_CURRENT_SOURCE_DIR}/BuiltInNodes/Audio
        ${CMAKE_CURRENT_SOURCE_DIR}/BuiltInNodes/Library
        ${CMAKE_CURRENT_SOURCE_DIR}/BuiltInNodes/Examples
        ${CMAKE_CURRENT_SOURCE_DIR}/BuiltInNodes/ThreadPull
        ${CMAKE_CURRENT_SOURCE_DIR}/BuiltInNodes/LLama
        ${DAQSTER_SOURCE_DIR}/src/plugins/capabilities
    COMPILE_DEFINITIONS
        NODE_EDITOR_SHARED
)

# ── Output name must contain "plugin" substring ──────────────────────
if(TARGET NodeEditorIde)
    set_target_properties(NodeEditorIde PROPERTIES
        OUTPUT_NAME "NodeEditorPluginIde"
    )
endif()
