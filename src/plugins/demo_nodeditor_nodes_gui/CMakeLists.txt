cmake_minimum_required(VERSION 3.16)

project(demo_nodeditor_nodes_gui VERSION 0.3.2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ── Plugin JSON metadata (generated from template) ──────────────────
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/DemoNodeEditorNodesGuiInterface.json.in"
    "${CMAKE_CURRENT_SOURCE_DIR}/DemoNodeEditorNodesGuiInterface.json"
    @ONLY
)

# ── Plugin infrastructure (root level) ──────────────────────────────
set(PLUGIN_SOURCES
    DemoNodeEditorNodesGuiInterface.cpp
    DemoNodeEditorNodesGuiInterface.h
    DemoNodeEditorNodesGuiObject.cpp
    DemoNodeEditorNodesGuiObject.h
)

# ── GUI Widgets for core models ─────────────────────────────────────
# Each core model gets a corresponding GUI widget
set(GUI_WIDGETS
    Sources/AudioSource/AudioSourceDataModelUI.h
    Sources/AudioSource/AudioSourceDataModelUI.cpp
    Sources/AudioSource/AudioSourceDataModelUI.ui
    Sources/AudioSource/AudioSourceConfig.ui
    Sources/LLamaSource/ChatBaseWidget.h
    Sources/LLamaSource/ChatBaseWidget.cpp
    Sources/Video/VideoGLBlitWidget.h
    Sources/Video/VideoGLBlitWidget.cpp
    Sources/Video/VideoPerfBadge.h
    Sources/Video/VideoPerfBadge.cpp
    Sources/PlutoSdr/PlutoSdrWidget.h
    Sources/PlutoSdr/PlutoSdrWidget.cpp
    Sources/SystemMonitor/SystemMonitorWidget.h
    Sources/SystemMonitor/SystemMonitorWidget.cpp
    Sources/Gamepad/GamepadWidget.h
    Sources/Gamepad/GamepadWidget.cpp
    Sources/FilePlayback/FilePlaybackWidget.h
    Sources/FilePlayback/FilePlaybackWidget.cpp
    Sources/NetworkSource/NetworkSourceWidget.h
    Sources/NetworkSource/NetworkSourceWidget.cpp
    Sinks/NetworkSink/NetworkSinkWidget.h
    Sinks/NetworkSink/NetworkSinkWidget.cpp
    Sinks/FileRecord/FileRecordWidget.h
    Sinks/FileRecord/FileRecordWidget.cpp
    Sources/GpuMonitor/GpuMonitorWidget.cpp
    Sources/JackDetect/JackDetectWidget.cpp
    Sources/Pcap/PcapWidget.cpp
)

# ── NodeWidgetFactory for core/gui connection ───────────────────────
set(FACTORY_SOURCES
    NodeWidgetFactory.h
    NodeWidgetFactory.cpp
)

# ── Use the Daqster plugin template ──────────────────────────────────
create_plugin(DemoNodeEditorNodesGui
    SOURCES
        ${PLUGIN_SOURCES}
        ${GUI_WIDGETS}
        ${FACTORY_SOURCES}
    REQUIRES_LIBRARIES
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Charts
        Qt${QT_VERSION_MAJOR}::Multimedia
        Qt${QT_VERSION_MAJOR}::MultimediaWidgets
        Qt${QT_VERSION_MAJOR}::Network
        Qt${QT_VERSION_MAJOR}::OpenGL
        QtNodes
        DaqsterCore
        DaqsterGui
        DemoNodeEditorNodesCore
    INCLUDE_DIRECTORIES
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${DAQSTER_SOURCE_DIR}/src/plugins/demo_nodeditor_nodes_core
        ${DAQSTER_SOURCE_DIR}/src/gui
        ${CMAKE_CURRENT_SOURCE_DIR}/Sources/AudioSource
        ${CMAKE_CURRENT_SOURCE_DIR}/Sources/LLamaSource
        ${CMAKE_CURRENT_SOURCE_DIR}/Sources/Video
        ${CMAKE_CURRENT_SOURCE_DIR}/Sources/PlutoSdr
        ${CMAKE_CURRENT_SOURCE_DIR}/Sources/SystemMonitor
        ${CMAKE_CURRENT_SOURCE_DIR}/Sources/Gamepad
        ${CMAKE_CURRENT_SOURCE_DIR}/Sources/FilePlayback
        ${CMAKE_CURRENT_SOURCE_DIR}/Sources/NetworkSource
        ${CMAKE_CURRENT_SOURCE_DIR}/Sinks/NetworkSink
        ${CMAKE_CURRENT_SOURCE_DIR}/Sinks/FileRecord
        ${CMAKE_CURRENT_SOURCE_DIR}/Sources/GpuMonitor
        ${CMAKE_CURRENT_SOURCE_DIR}/Sources/JackDetect
        ${CMAKE_CURRENT_SOURCE_DIR}/Sources/Pcap
        ${CMAKE_CURRENT_SOURCE_DIR}/Displays/DaqDisplay
)

# ── Output name must contain "plugin" substring ──────────────────────
if(TARGET DemoNodeEditorNodesGui)
    set_target_properties(DemoNodeEditorNodesGui PROPERTIES
        OUTPUT_NAME "DemoNodeEditorNodesGuiPlugin"
    )
endif()