cmake_minimum_required(VERSION 3.16)

project(nodeditor_nodes_gui VERSION 0.3.2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Windows: export all symbols so the plugin DLL can be loaded by QPluginLoader
if(WIN32)
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()

set(DEMO_GUI_RUNTIME_LIBS)
if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    list(APPEND DEMO_GUI_RUNTIME_LIBS
        Qt${QT_VERSION_MAJOR}::MultimediaWidgets
        Qt${QT_VERSION_MAJOR}::OpenGLWidgets
    )
endif()

# ── Plugin JSON metadata (generated from template) ──────────────────
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/NodeditorNodesGuiInterface.json.in"
    "${CMAKE_CURRENT_SOURCE_DIR}/NodeditorNodesGuiInterface.json"
    @ONLY
)

# ── Plugin infrastructure (root level) ──────────────────────────────
set(PLUGIN_SOURCES
    NodeditorNodesGuiInterface.cpp
    NodeditorNodesGuiInterface.h
    NodeditorNodesGuiObject.cpp
    NodeditorNodesGuiObject.h
)

# ── GUI Widgets for core models ─────────────────────────────────────
# Each core model gets a corresponding GUI widget
set(GUI_WIDGETS
    Sources/PlutoSdr/PlutoSdrWidget.h
    Sources/PlutoSdr/PlutoSdrWidget.cpp
    Sources/NumberSourceWidget.h
    Sources/NumberSourceWidget.cpp
    Sources/Gamepad/GamepadWidget.h
    Sources/Gamepad/GamepadWidget.cpp
    Sources/AudioSource/AudioSourceDataModelUI.h
    Sources/AudioSource/AudioSourceDataModelUI.cpp
    Sources/AudioSource/AudioSourceDataModelUI.ui
    Sources/AudioSource/AudioSourceConfig.h
    Sources/AudioSource/AudioSourceConfig.cpp
    Sources/AudioSource/AudioSourceConfig.ui
    Sources/SystemMonitor/SystemMonitorWidget.h
    Sources/SystemMonitor/SystemMonitorWidget.cpp
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
      Displays/NumberDisplay/NumberDisplayWidget.h
      Displays/NumberDisplay/NumberDisplayWidget.cpp
    Operators/ModuloWidget.h
    Operators/ModuloWidget.cpp
    Operators/ArithmeticLogicWidget.h
    Operators/ArithmeticLogicWidget.cpp
    Sources/LLamaSource/ChatBaseWidget.h
    Sources/LLamaSource/ChatBaseWidget.cpp
    Sources/LLamaSource/ConsoleWidget.h
    Sources/LLamaSource/ConsoleWidget.cpp
    Sources/LLamaSource/LLamaModelWidget.h
    Sources/LLamaSource/LLamaModelWidget.cpp
    Displays/DaqDisplay/DaqDisplayWidget.h
    Displays/DaqDisplay/DaqDisplayWidget.cpp
    # REQ-SW-PL-056: the obsolete QDevIO display VIEW moved here from
    # node_editor_ide/NodeSupport/display — NodeWidgetFactory's
    # "AudioDisplayObsolete" creator builds it and connects the model's device
    # bufferReady + audioConnectionChanged + audioViewConfigChanged signals.
    Displays/ObsoleteAudioDisplay/QDevioDisplayModelUiObsolete.h
    Displays/ObsoleteAudioDisplay/QDevioDisplayModelUiObsolete.cpp
    Displays/ObsoleteAudioDisplay/QDevioDisplayModelUiObsolete.ui
    Sources/Video/CameraSourceWidget.h
    Sources/Video/CameraSourceWidget.cpp
    Sources/Video/StreamSourceWidget.h
    Sources/Video/StreamSourceWidget.cpp
    Sources/Video/VideoFileSourceWidget.h
    Sources/Video/VideoFileSourceWidget.cpp
    Processing/Video/FrameSamplerWidget.h
    Processing/Video/FrameSamplerWidget.cpp
    Processing/Video/CustomShaderWidget.h
    Processing/Video/CustomShaderWidget.cpp
    Processing/Video/VideoEffectWidget.h
    Processing/Video/VideoEffectWidget.cpp
    # REQ-SW-PL-055: the software display backend. VideoDisplayWidget itself
    # lives in nodeditor_nodes/core — it is a pure interface with no
    # QWidget, and VideoOutputNode in _core holds a VideoDisplayWidget* to push
    # frames through, so the interface has to be nameable from _core. This
    # plugin supplies the implementations. VideoGLBlitWidget arrives in the next
    # block and combines the same interface with its own widget base.
    Sinks/Video/VideoSoftwareWidget.h
    Sinks/Video/VideoSoftwareWidget.cpp
    # The GL backend plus the Video Output node's whole widget tree. These three
    # used to be compiled into nodeditor_nodes/core (its EMBEDDED_WIDGETS
    # list) because VideoOutputNode constructed them directly. It no longer does:
    # the model holds an IVideoOutputUi supplied by IWidgetProvider, and this
    # plugin implements it.
    Sinks/Video/VideoGLBlitWidget.h
    Sinks/Video/VideoGLBlitWidget.cpp
    Sinks/Video/VideoOutputUi.h
    Sinks/Video/VideoOutputUi.cpp
)

# ── NodeWidgetFactory for core/gui connection ───────────────────────
set(FACTORY_SOURCES
    NodeWidgetFactory.h
    NodeWidgetFactory.cpp
)

# ── Use the Daqster plugin template ──────────────────────────────────
create_plugin(NodeditorNodesGui
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
        FrameworkCore
        FrameworkGui
        NodeditorNodesCore
    INCLUDE_DIRECTORIES
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${DAQSTER_SOURCE_DIR}/src/plugins/nodeditor_nodes/core
        ${DAQSTER_SOURCE_DIR}/src/plugins/nodeditor_nodes/core/Sources/AudioSource
        ${DAQSTER_SOURCE_DIR}/src/plugins/nodeditor_nodes/core/Sources/Video
        ${DAQSTER_SOURCE_DIR}/src/plugins/nodeditor_nodes/core/Sinks/Video
        ${DAQSTER_SOURCE_DIR}/src/plugins/nodeditor_nodes/core/Processing/Video
        ${DAQSTER_SOURCE_DIR}/src/plugins/nodeditor_nodes/core/Sources/GpuMonitor
        ${DAQSTER_SOURCE_DIR}/src/plugins/nodeditor_nodes/core/Sources/JackDetect
        ${DAQSTER_SOURCE_DIR}/src/plugins/nodeditor_nodes/core/Sources/Pcap
        ${DAQSTER_SOURCE_DIR}/src/plugins/nodeditor_nodes/core/Sources/SystemMonitor
        ${DAQSTER_SOURCE_DIR}/src/plugins/nodeditor_nodes/core/Sources/PlutoSdr
        ${DAQSTER_SOURCE_DIR}/src/frame_work/base/src/include
        ${DAQSTER_SOURCE_DIR}/src/frame_work/base/src/logging
        ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor_ide/NodeSupport
        ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor_ide/NodeSupport/display
        ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor_ide/NodeSupport/connectors
        ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor_ide/NodeSupport/decoders
        ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor_ide/NodeSupport/types
        ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor_ide/NodeSupport/threading
        ${CMAKE_CURRENT_SOURCE_DIR}/Sources/AudioSource
        ${CMAKE_CURRENT_SOURCE_DIR}/Sources/LLamaSource
        ${CMAKE_CURRENT_SOURCE_DIR}/Sources/Video
        ${CMAKE_CURRENT_SOURCE_DIR}/Processing/Video
        ${CMAKE_CURRENT_SOURCE_DIR}/Sinks/Video
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
if(TARGET NodeditorNodesGui)
    set_target_properties(NodeditorNodesGui PROPERTIES
        OUTPUT_NAME "NodeditorNodesGuiPlugin"
    )
    # Windows: export all symbols from the plugin DLL
    if(WIN32)
        set_target_properties(NodeditorNodesGui PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
    endif()
endif()