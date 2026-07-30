cmake_minimum_required(VERSION 3.16)

project(demo_nodeditor_nodes VERSION 0.3.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# ── Plugin infrastructure (root level) ──────────────────────────────
set(PLUGIN_SOURCES
    DemoNodeEditorNodesInterface.cpp
    DemoNodeEditorNodesInterface.h
    DemoNodeEditorNodesObject.cpp
    DemoNodeEditorNodesObject.h
)

# ── Sources: AudioSource ────────────────────────────────────────────
set(AUDIO_SOURCE_NODES
    Sources/AudioSource/AudioSourceDataModel.h
    Sources/AudioSource/AudioSourceDataModel.cpp
    Sources/AudioSource/AudioSourceDataModelUI.h
    Sources/AudioSource/AudioSourceDataModelUI.cpp
    Sources/AudioSource/AudioSourceDataModelUI.ui
    Sources/AudioSource/AudioSourceConfig.h
    Sources/AudioSource/AudioSourceConfig.cpp
    Sources/AudioSource/AudioSourceConfig.ui
    Sources/AudioSource/AudioWorker.h
    Sources/AudioSource/AudioWorker.cpp
    Sources/AudioSource/AudioNodeQdevIoConnector.h
    Sources/AudioSource/AudioNodeQdevIoConnector.cpp
    Sources/AudioSource/AudioComboModel.h
    Sources/AudioSource/AudioComboModel.cpp
    Sources/AudioSource/node_editor.qrc
)

# ── Sources: LLamaSource ────────────────────────────────────────────
set(LLAMA_SOURCE_NODES
    Sources/LLamaSource/LLamaModelDataModel.h
    Sources/LLamaSource/LLamaModelDataModel.cpp
    Sources/LLamaSource/ConsoleDataModel.h
    Sources/LLamaSource/ConsoleDataModel.cpp
    Sources/LLamaSource/ChatBaseWidget.h
    Sources/LLamaSource/ChatBaseWidget.cpp
)

# ── Displays: AudioDisplay ──────────────────────────────────────────
set(AUDIO_DISPLAY_NODES
    Displays/AudioDisplay/AudioDisplayModel.h
    Displays/AudioDisplay/AudioDisplayModel.cpp
)

# ── Displays: GenericDisplay ────────────────────────────────────────
set(GENERIC_DISPLAY_NODES
    Displays/GenericDisplay/GenericDisplayNode.h
    Displays/GenericDisplay/GenericDisplayNode.cpp
)

# ── Routing: Demux + Mux ───────────────────────────────────────────
set(ROUTING_NODES
    Routing/Demux/DemuxNode.h
    Routing/Demux/DemuxNode.cpp
    Routing/Mux/MuxNode.h
    Routing/Mux/MuxNode.cpp
)

# ── Shared types ────────────────────────────────────────────────────
set(SHARED_TYPES
)

# ── Use the Daqster plugin template ──────────────────────────────────
create_plugin(DemoNodeEditorNodes
    SOURCES
        ${PLUGIN_SOURCES}
        ${AUDIO_SOURCE_NODES}
        ${LLAMA_SOURCE_NODES}
        ${GENERIC_DISPLAY_NODES}
        ${AUDIO_DISPLAY_NODES}
        ${ROUTING_NODES}
        ${SHARED_TYPES}
    REQUIRES_LIBRARIES
        NodeEditorLibrary
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        Qt${QT_VERSION_MAJOR}::Charts
        Qt${QT_VERSION_MAJOR}::Multimedia
        Qt${QT_VERSION_MAJOR}::Network
        Qt${QT_VERSION_MAJOR}::OpenGL
        QtNodes
        frame_work
    INCLUDE_DIRECTORIES
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${DAQSTER_SOURCE_DIR}/src/plugins/common
        ${CMAKE_CURRENT_SOURCE_DIR}/Sources/AudioSource
        ${CMAKE_CURRENT_SOURCE_DIR}/Sources/LLamaSource
        ${CMAKE_CURRENT_SOURCE_DIR}/Displays/AudioDisplay
)

# ── Output name must contain "plugin" substring ──────────────────────
if(TARGET DemoNodeEditorNodes)
    set_target_properties(DemoNodeEditorNodes PROPERTIES
        OUTPUT_NAME "DemoNodeEditorNodesPlugin"
    )
endif()
