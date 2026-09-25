cmake_minimum_required(VERSION 3.16)

project(demo_nodeditor_nodes_gui VERSION 0.3.2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(DEMO_GUI_RUNTIME_LIBS)
if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    list(APPEND DEMO_GUI_RUNTIME_LIBS
        Qt${QT_VERSION_MAJOR}::MultimediaWidgets
        Qt${QT_VERSION_MAJOR}::OpenGLWidgets
    )
endif()

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
    Sources/GpuMonitor/GpuMonitorWidget.h
    Sources/GpuMonitor/GpuMonitorWidget.cpp
    Sources/JackDetect/JackDetectWidget.h
    Sources/JackDetect/JackDetectWidget.cpp
    Sources/Pcap/PcapWidget.h
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
        ${DEMO_GUI_RUNTIME_LIBS}
        Qt${QT_VERSION_MAJOR}::Network
        Qt${QT_VERSION_MAJOR}::OpenGL
        QtNodes
        DaqsterCore
        DaqsterGui
        DemoNodeEditorNodesCore
    INCLUDE_DIRECTORIES
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${DAQSTER_SOURCE_DIR}/src/plugins/demo_nodeditor_nodes_core
        ${DAQSTER_SOURCE_DIR}/src/plugins/demo_nodeditor_nodes_core/Sources/AudioSource
        ${DAQSTER_SOURCE_DIR}/src/plugins/demo_nodeditor_nodes_core/Sources/Video
        ${DAQSTER_SOURCE_DIR}/src/plugins/demo_nodeditor_nodes_core/Sources/GpuMonitor
        ${DAQSTER_SOURCE_DIR}/src/plugins/demo_nodeditor_nodes_core/Sources/JackDetect
        ${DAQSTER_SOURCE_DIR}/src/plugins/demo_nodeditor_nodes_core/Sources/Pcap
        ${DAQSTER_SOURCE_DIR}/src/plugins/demo_nodeditor_nodes_core/Sources/SystemMonitor
        ${DAQSTER_SOURCE_DIR}/src/plugins/demo_nodeditor_nodes_core/Sources/PlutoSdr
        ${DAQSTER_SOURCE_DIR}/src/frame_work/base/src/include
        ${DAQSTER_SOURCE_DIR}/src/frame_work/base/src/logging
        ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor_ide/BuiltInNodes/Library
        ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor_ide/BuiltInNodes/Library/display
        ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor_ide/BuiltInNodes/Library/connectors
        ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor_ide/BuiltInNodes/Library/decoders
        ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor_ide/BuiltInNodes/Library/types
        ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor_ide/BuiltInNodes/Library/threading
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