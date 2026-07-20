cmake_minimum_required(VERSION 3.16)

project(node_editor_ide VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# ── Library: Core types ─────────────────────────────────────────────
set(LIBRARY_TYPES
    BuiltInNodes/Library/types/IStreamDecoder.h
    BuiltInNodes/Library/types/QDevIOStreamConfig.h
    BuiltInNodes/Library/types/StreamChannel.h
    BuiltInNodes/Library/types/GenericNumericTypes.h
    BuiltInNodes/Library/types/GenericNumericTypes.cpp
)

# ── Library: Connectors ─────────────────────────────────────────────
set(LIBRARY_CONNECTORS
    BuiltInNodes/Library/connectors/NodeDataModelToQIODeviceConnector.h
    BuiltInNodes/Library/connectors/NodeDataModelToQIODeviceConnector.cpp
    BuiltInNodes/Library/connectors/GenericQDevIoConnector.h
    BuiltInNodes/Library/connectors/GenericQDevIoConnector.cpp
)

# ── Library: Decoders ───────────────────────────────────────────────
set(LIBRARY_DECODERS
    BuiltInNodes/Library/decoders/AudioFrameDecoder.h
    BuiltInNodes/Library/decoders/AudioFrameDecoder.cpp
)

# ── Library: Display ────────────────────────────────────────────────
set(LIBRARY_DISPLAY
    BuiltInNodes/Library/display/QDevIoDisplayModel.h
    BuiltInNodes/Library/display/QDevIoDisplayModel.cpp
    BuiltInNodes/Library/display/QDevioDisplayModelUi.h
    BuiltInNodes/Library/display/QDevioDisplayModelUi.cpp
    BuiltInNodes/Library/display/QDevioDisplayModelUi.ui
    BuiltInNodes/Library/display/XYSeriesIODevice.h
    BuiltInNodes/Library/display/XYSeriesIODevice.cpp
    BuiltInNodes/Library/display/QtChartsCompat.h
)

# ── Library: Threading ──────────────────────────────────────────────
set(LIBRARY_THREADING
    BuiltInNodes/Library/threading/EventThreadPull.h
    BuiltInNodes/Library/threading/EventThreadPull.cpp
)

# ── Audio nodes (domain-specific, stays here for now) ────────────────
set(AUDIO_NODES
    BuiltInNodes/Audio/AudioNodeQdevIoConnector.h
    BuiltInNodes/Audio/AudioNodeQdevIoConnector.cpp
    BuiltInNodes/Audio/AudioComboModel.h
    BuiltInNodes/Audio/AudioComboModel.cpp
    BuiltInNodes/Audio/AudioSourceConfig.h
    BuiltInNodes/Audio/AudioSourceConfig.cpp
    BuiltInNodes/Audio/AudioSourceConfig.ui
    BuiltInNodes/Audio/AudioSourceDataModel.h
    BuiltInNodes/Audio/AudioSourceDataModel.cpp
    BuiltInNodes/Audio/AudioSourceDataModelUI.h
    BuiltInNodes/Audio/AudioSourceDataModelUI.cpp
    BuiltInNodes/Audio/AudioSourceDataModelUI.ui
    BuiltInNodes/Audio/AudioWorker.h
    BuiltInNodes/Audio/AudioWorker.cpp
    BuiltInNodes/Audio/AudioCompat.h
    BuiltInNodes/Audio/node_editor.qrc
)

# ── LLama nodes (domain-specific, stays here for now) ───────────────
set(LLAMA_NODES
    BuiltInNodes/LLama/LLamaModelDataModel.h
    BuiltInNodes/LLama/LLamaModelDataModel.cpp
    BuiltInNodes/LLama/ConsoleDataModel.h
    BuiltInNodes/LLama/ConsoleDataModel.cpp
    BuiltInNodes/LLama/ChatBaseWidget.h
    BuiltInNodes/LLama/ChatBaseWidget.cpp
    BuiltInNodes/LLama/TextData.h
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
        ${LIBRARY_TYPES}
        ${LIBRARY_CONNECTORS}
        ${LIBRARY_DECODERS}
        ${LIBRARY_DISPLAY}
        ${LIBRARY_THREADING}
        ${AUDIO_NODES}
        ${LLAMA_NODES}
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
        ${CMAKE_CURRENT_SOURCE_DIR}/BuiltInNodes/Library/types
        ${CMAKE_CURRENT_SOURCE_DIR}/BuiltInNodes/Library/connectors
        ${CMAKE_CURRENT_SOURCE_DIR}/BuiltInNodes/Library/decoders
        ${CMAKE_CURRENT_SOURCE_DIR}/BuiltInNodes/Library/display
        ${CMAKE_CURRENT_SOURCE_DIR}/BuiltInNodes/Library/threading
        ${CMAKE_CURRENT_SOURCE_DIR}/BuiltInNodes/Audio
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
