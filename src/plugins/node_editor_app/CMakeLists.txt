cmake_minimum_required(VERSION 3.16)

project(node_editor_app VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# ── Node model sources (moved from old monolithic node_editor plugin) ─
set(NODE_APP_SOURCES
    NodeEditorAppInterface.cpp
    NodeEditorAppInterface.h
    NodeEditorAppObject.cpp
    NodeEditorAppObject.h
    Audio/AudioNodeQdevIoConnector.cpp
    Audio/AudioComboModel.cpp
    Audio/AudioSourceConfig.cpp
    Audio/AudioSourceDataModel.cpp
    Audio/AudioSourceDataModelUI.cpp
    Audio/AudioWorker.cpp
    Library/NodeDataModelToQIODeviceConnector.cpp
    Examples/Converters.cpp
    Examples/ModuloModel.cpp
    Examples/NumberDisplayDataModel.cpp
    Examples/NumberSourceDataModel.cpp
    Examples/NumberSourceDataUi.cpp
    ThreadPull/EventThreadPull.cpp
    Library/QDevIoDisplayModel.cpp
    Library/QDevioDisplayModelUi.cpp
    Library/AudioFrameDecoder.cpp
    Library/XYSeriesIODevice.cpp
    LLama/LLamaModelDataModel.cpp
    LLama/ConsoleDataModel.cpp
    LLama/ChatBaseWidget.cpp
    Audio/AudioNodeQdevIoConnector.h
    Audio/AudioComboModel.h
    Audio/AudioSourceConfig.h
    Audio/AudioSourceDataModel.h
    Audio/AudioSourceDataModelUI.h
    Audio/AudioWorker.h
    Library/NodeDataModelToQIODeviceConnector.h
    Examples/Converters.h
    Examples/ModuloModel.h
    Examples/NumberDisplayDataModel.h
    Examples/NumberSourceDataModel.h
    Examples/NumberSourceDataUi.h
    Examples/DecimalData.h
    Examples/IntegerData.h
    ThreadPull/EventThreadPull.h
    Library/ComplexType.h
    Library/AudioFrameDecoder.h
    Library/QDevIoDisplayModel.h
    Library/QDevioDisplayModelUi.h
    Library/XYSeriesIODevice.h
    LLama/TextData.h
    LLama/LLamaModelDataModel.h
    LLama/ConsoleDataModel.h
    LLama/ChatBaseWidget.h
    Audio/AudioSourceConfig.ui
    Audio/AudioSourceDataModelUI.ui
    Examples/NumberSourceDataUi.ui
    Library/QDevioDisplayModelUi.ui
    Audio/node_editor.qrc
)

# ── Use the Daqster plugin template ──────────────────────────────────
create_plugin(NodeEditorApp
    SOURCES
        ${NODE_APP_SOURCES}
    REQUIRES_LIBRARIES
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        Qt${QT_VERSION_MAJOR}::Network
        Qt${QT_VERSION_MAJOR}::Multimedia
        Qt${QT_VERSION_MAJOR}::Charts
        Qt${QT_VERSION_MAJOR}::OpenGL
        QtNodes
        node_editor_widget
        frame_work
    INCLUDE_DIRECTORIES
        ${CMAKE_CURRENT_SOURCE_DIR}/Audio
        ${CMAKE_CURRENT_SOURCE_DIR}/Library
        ${CMAKE_CURRENT_SOURCE_DIR}/Examples
        ${CMAKE_CURRENT_SOURCE_DIR}/ThreadPull
        ${CMAKE_CURRENT_SOURCE_DIR}/LLama
    COMPILE_DEFINITIONS
        NODE_EDITOR_SHARED
)

# ── Output name must contain "plugin" substring ──────────────────────
# QPluginManager::IsCandidatePluginFile() requires the .so filename
# to contain "plugin". The old target was "NodeEditorPlugin", which
# matched. Renaming to "NodeEditorPluginApp" preserves discoverability.
set_target_properties(NodeEditorApp PROPERTIES
    OUTPUT_NAME "NodeEditorPluginApp"
)
