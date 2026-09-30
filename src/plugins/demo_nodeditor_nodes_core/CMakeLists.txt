cmake_minimum_required(VERSION 3.16)

project(demo_nodeditor_nodes_core VERSION 0.3.2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# Provides generate_export_header() for demo_nodeditor_nodes_core_export.h.
# The module sets its own directory-scoped path variable, so it has to be
# included here and not only where another target happens to use it.
include(GenerateExportHeader)

# Windows: export all symbols so the plugin DLL can be loaded by QPluginLoader
if(WIN32)
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()

set(DEMO_CORE_GUI_LIBS)
if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    list(APPEND DEMO_CORE_GUI_LIBS
        Qt${QT_VERSION_MAJOR}::MultimediaWidgets
        Qt${QT_VERSION_MAJOR}::OpenGLWidgets
    )
endif()

# ── Plugin JSON metadata (generated from template) ──────────────────
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/DemoNodeEditorNodesCoreInterface.json.in"
    "${CMAKE_CURRENT_SOURCE_DIR}/DemoNodeEditorNodesCoreInterface.json"
    @ONLY
)

# ── Plugin infrastructure (root level) ──────────────────────────────
set(PLUGIN_SOURCES
    DemoNodeEditorNodesCoreInterface.cpp
    DemoNodeEditorNodesCoreInterface.h
    DemoNodeEditorNodesCoreObject.cpp
    DemoNodeEditorNodesCoreObject.h
)

# ── Sources: AudioSource ────────────────────────────────────────────
set(AUDIO_SOURCE_NODES
    Sources/AudioSource/AudioSourceDataModel.h
    Sources/AudioSource/AudioSourceDataModel.cpp
    Sources/AudioSource/MicCaptureWorker.h
    Sources/AudioSource/MicCaptureWorker.cpp
    Sources/AudioSource/AudioComboModel.h
    Sources/AudioSource/AudioComboModel.cpp
    # Obsolete audio models (migrated from old plugin)
    Sources/AudioSource/AudioSourceDataModelObsolete.h
    Sources/AudioSource/AudioSourceDataModelObsolete.cpp
    Sources/AudioSource/AudioWorkerObsolete.h
    Sources/AudioSource/AudioWorkerObsolete.cpp
    Sources/AudioSource/AudioStartStop.h
    Sources/AudioSource/AudioNodeQdevIoConnectorObsolete.h
    Sources/AudioSource/AudioNodeQdevIoConnectorObsolete.cpp
)

# ── Sources: LLamaSource ────────────────────────────────────────────
set(LLAMA_SOURCE_NODES
    Sources/LLamaSource/LLamaModelDataModel.h
    Sources/LLamaSource/LLamaModelDataModel.cpp
    Sources/LLamaSource/ConsoleDataModel.h
    Sources/LLamaSource/ConsoleDataModel.cpp
)

# ── Video nodes (core models only, no GUI widgets) ──────────────────
set(VIDEO_NODES
    Sources/Video/VideoCompat.h
    Sources/Video/CameraSourceNode.h
    Sources/Video/CameraSourceNode.cpp
    Sources/Video/VideoFileSourceNode.h
    Sources/Video/VideoFileSourceNode.cpp
    Sources/Video/StreamSourceNode.h
    Sources/Video/StreamSourceNode.cpp
    Sources/Video/StreamUrlValidator.h
    Sources/Video/StreamUrlValidator.cpp
    Sources/Video/VideoOutputNode.h
    Sources/Video/VideoOutputNode.cpp
    Sources/Video/VideoPerfBadge.h
    Sources/Video/VideoPerfBadge.cpp
    Sources/Video/VideoTransformOps.h
    Sources/Video/VideoTransformOps.cpp
    Sources/Video/VideoGLShaders.h
    Sources/Video/VideoEffectOps.h
    Sources/Video/VideoEffectOps.cpp
    Sources/Video/VideoEffectGLProcessor.h
    Sources/Video/VideoEffectGLProcessor.cpp
    Sources/Video/VideoEffectNode.h
    Sources/Video/VideoEffectNode.cpp
    Sources/Video/CustomShaderGLProcessor.h
    Sources/Video/CustomShaderGLProcessor.cpp
    Sources/Video/CustomShaderNode.h
    Sources/Video/CustomShaderNode.cpp
    Sources/Video/FrameSamplerNode.h
    Sources/Video/FrameSamplerNode.cpp
    # Texture pool for GPU effect outputs (REQ-SW-PL-032 Issue #7)
    ${DAQSTER_SOURCE_DIR}/src/plugins/common/GL/TexturePool.cpp
    # Common compute thread pool (REQ-SW-PL-039)
    ${DAQSTER_SOURCE_DIR}/src/plugins/common/Threading/ComputePool.cpp
)

# ── Optional OpenCV transforms ──────────────────────────────────────
option(DAQSTER_USE_OPENCV "Enable OpenCV-based video transform operations" ON)
find_package(OpenCV QUIET)
set(VIDEO_OPENCV_SOURCES)
set(VIDEO_OPENCV_DEFINITIONS)
set(VIDEO_OPENCV_INCLUDE_DIRS)
set(VIDEO_OPENCV_LINK_LIBRARIES)
if(DAQSTER_USE_OPENCV AND OpenCV_FOUND)
    message(STATUS "demo_nodeditor_nodes_core: OpenCV ${OpenCV_VERSION} found — enabling OpenCV video transform operations")
    list(APPEND VIDEO_OPENCV_SOURCES Sources/Video/OpenCVTransforms.cpp)
    list(APPEND VIDEO_OPENCV_DEFINITIONS HAVE_OPENCV)
    list(APPEND VIDEO_OPENCV_INCLUDE_DIRS ${OpenCV_INCLUDE_DIRS})
    list(APPEND VIDEO_OPENCV_LINK_LIBRARIES ${OpenCV_LIBS})
else()
    message(STATUS "demo_nodeditor_nodes_core: OpenCV not found or disabled — using the 8 base video transform operations")
endif()

# ── Optional libiio PlutoSDR node ───────────────────────────────────
# NOTE ON THE SIX OPTIONAL NODE FAMILIES BELOW (PlutoSDR, System Monitor,
# Gamepad, GPU Monitor, Jack Detect, Pcap): their *_SOURCES lists are NOT
# conditional, only their *_DEFINITIONS / *_INCLUDE_DIRS / *_LINK_LIBRARIES
# are. Every one of these translation units already guards its platform or
# third-party calls with #ifdef HAVE_* and degrades to a runtime error (or a
# no-op) when the dependency is missing, so the class must exist in the DLL
# unconditionally. Dropping the sources instead leaves the class undefined,
# and the consumers that reference it — this plugin's own node registry and
# the GUI plugin's NodeWidgetFactory — then fail to link. Linux tolerates that
# (undefined symbols in a shared object are resolved lazily at dlopen), but
# the Windows linker reports it as LNK2019/LNK1120. The node is still only
# *registered* when the HAVE_* macro is defined, so the node palette stays
# platform-correct; only the compiled surface is unconditional.
set(PLUTOSDR_SOURCES
    Sources/PlutoSdr/PlutoSdrEngine.h
    Sources/PlutoSdr/PlutoSdrEngine.cpp
    Sources/PlutoSdr/PlutoSdrModel.h
    Sources/PlutoSdr/PlutoSdrModel.cpp
)
set(PLUTOSDR_DEFINITIONS)
set(PLUTOSDR_INCLUDE_DIRS)
set(PLUTOSDR_LINK_LIBRARIES)
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(IIO QUIET libiio)
endif()
if(NOT IIO_FOUND)
    find_package(libiio QUIET)
endif()
if(IIO_FOUND)
    message(STATUS "demo_nodeditor_nodes_core: libiio ${IIO_VERSION} found — enabling PlutoSDR RX DAQ node")
    list(APPEND PLUTOSDR_DEFINITIONS HAVE_LIBIIO)
    list(APPEND PLUTOSDR_INCLUDE_DIRS ${IIO_INCLUDE_DIRS})
    if(TARGET IIO::IIO)
        list(APPEND PLUTOSDR_LINK_LIBRARIES IIO::IIO)
    else()
        list(APPEND PLUTOSDR_LINK_LIBRARIES ${IIO_LIBRARIES})
    endif()
else()
    message(STATUS "demo_nodeditor_nodes_core: libiio not found — PlutoSDR RX DAQ node disabled")
endif()

# ── Optional System Monitor source node ─────────────────────────────
# Always compiled — see the note on the optional node families above.
set(SYSTEM_MONITOR_SOURCES
    Sources/SystemMonitor/SystemMonitorEngine.h
    Sources/SystemMonitor/SystemMonitorEngine.cpp
    Sources/SystemMonitor/SystemMonitorModel.h
    Sources/SystemMonitor/SystemMonitorModel.cpp
)
set(SYSTEM_MONITOR_DEFINITIONS)
if(NOT WIN32)
    message(STATUS "demo_nodeditor_nodes_core: System Monitor source node enabled (Linux /proc + /sys)")
    list(APPEND SYSTEM_MONITOR_DEFINITIONS HAVE_SYSTEM_MONITOR)
else()
    message(STATUS "demo_nodeditor_nodes_core: System Monitor source node disabled (Windows)")
endif()

# ── Optional Gamepad source node ────────────────────────────────────
# Always compiled — see the note on the optional node families above.
# GamepadEngine.cpp needs its Linux-only includes and poll()/open() bodies
# wrapped in #ifdef HAVE_GAMEPAD to build on Windows; see that file.
set(GAMEPAD_SOURCES
    Sources/Gamepad/GamepadEngine.h
    Sources/Gamepad/GamepadEngine.cpp
    Sources/Gamepad/GamepadModel.h
    Sources/Gamepad/GamepadModel.cpp
)
set(GAMEPAD_DEFINITIONS)
if(NOT WIN32)
    message(STATUS "demo_nodeditor_nodes_core: Gamepad source node enabled (Linux joystick API)")
    list(APPEND GAMEPAD_DEFINITIONS HAVE_GAMEPAD)
else()
    message(STATUS "demo_nodeditor_nodes_core: Gamepad source node disabled (Windows)")
endif()

# ── File Record sink + File Playback source ─────────────────────────
set(FILE_IO_NODES
    Sinks/FileRecord/FileRecordModel.h
    Sinks/FileRecord/FileRecordModel.cpp
    Sources/FilePlayback/FilePlaybackModel.h
    Sources/FilePlayback/FilePlaybackModel.cpp
)

# ── Network Source + Sink ───────────────────────────────────────────
set(NETWORK_NODES
    Sources/NetworkSource/NetworkSourceModel.h
    Sources/NetworkSource/NetworkSourceModel.cpp
    Sinks/NetworkSink/NetworkSinkModel.h
    Sinks/NetworkSink/NetworkSinkModel.cpp
    shared/NetworkFrame.h
    shared/IStoppable.h
    shared/IStartable.h
)

# ── Optional NVML GPU Monitor source ────────────────────────────────
# Always compiled — see the note on the optional node families above.
set(GPU_MONITOR_SOURCES
    Sources/GpuMonitor/GpuMonitorEngine.h
    Sources/GpuMonitor/GpuMonitorEngine.cpp
    Sources/GpuMonitor/GpuMonitorModel.h
    Sources/GpuMonitor/GpuMonitorModel.cpp
)
set(GPU_MONITOR_DEFINITIONS)
set(GPU_MONITOR_INCLUDE_DIRS)
set(GPU_MONITOR_LINK_LIBRARIES)
find_library(NVML_LIBRARY NAMES nvidia-ml)
find_path(NVML_INCLUDE_DIR NAMES nvml.h)
if(NVML_LIBRARY AND NVML_INCLUDE_DIR)
    message(STATUS "demo_nodeditor_nodes_core: NVML found — enabling GPU Monitor source node")
    list(APPEND GPU_MONITOR_DEFINITIONS HAVE_NVML)
    list(APPEND GPU_MONITOR_INCLUDE_DIRS ${NVML_INCLUDE_DIR})
    list(APPEND GPU_MONITOR_LINK_LIBRARIES ${NVML_LIBRARY})
else()
    message(STATUS "demo_nodeditor_nodes_core: NVML not found — GPU Monitor source node disabled")
endif()

# ── Optional Jack-detect source ─────────────────────────────────────
# Always compiled — see the note on the optional node families above.
set(JACK_DETECT_SOURCES
    Sources/JackDetect/JackDetectEngine.h
    Sources/JackDetect/JackDetectEngine.cpp
    Sources/JackDetect/JackDetectModel.h
    Sources/JackDetect/JackDetectModel.cpp
)
set(JACK_DETECT_DEFINITIONS)
if(NOT WIN32)
    message(STATUS "demo_nodeditor_nodes_core: Jack-detect source node enabled (Linux HDA /proc/asound)")
    list(APPEND JACK_DETECT_DEFINITIONS HAVE_JACK_DETECT)
else()
    message(STATUS "demo_nodeditor_nodes_core: Jack-detect source node disabled (Windows)")
endif()

# ── Optional libpcap Packet Capture source ──────────────────────────
# Always compiled — see the note on the optional node families above.
# PcapEngine.h/.cpp already carry a void*-based fallback for the missing
# HAVE_PCAP case, so the class builds without libpcap.
set(PCAP_SOURCES
    Sources/Pcap/PcapEngine.h
    Sources/Pcap/PcapEngine.cpp
    Sources/Pcap/PcapModel.h
    Sources/Pcap/PcapModel.cpp
)
set(PCAP_DEFINITIONS)
set(PCAP_INCLUDE_DIRS)
set(PCAP_LINK_LIBRARIES)
if(WIN32)
    find_library(PCAP_LIBRARY NAMES wpcap Packet)
    find_path(PCAP_INCLUDE_DIR NAMES pcap.h)
else()
    find_library(PCAP_LIBRARY NAMES pcap)
    find_path(PCAP_INCLUDE_DIR NAMES pcap/pcap.h)
endif()
if(PCAP_LIBRARY AND PCAP_INCLUDE_DIR)
    message(STATUS "demo_nodeditor_nodes_core: libpcap found — enabling Pcap Capture source node")
    list(APPEND PCAP_DEFINITIONS HAVE_PCAP)
    list(APPEND PCAP_INCLUDE_DIRS ${PCAP_INCLUDE_DIR})
    list(APPEND PCAP_LINK_LIBRARIES ${PCAP_LIBRARY})
else()
    message(STATUS "demo_nodeditor_nodes_core: libpcap not found — Pcap Capture source node disabled")
endif()

# ── Displays: DAQ Display (core model only) ─────────────────────────
set(DAQ_DISPLAY_NODES
    Displays/DaqDisplay/DaqDisplayNode.h
    Displays/DaqDisplay/DaqDisplayNode.cpp
    # Migrated from old plugin
    Displays/GenericDisplay/GenericDisplayNode.h
    Displays/GenericDisplay/GenericDisplayNode.cpp
    Displays/AudioDisplay/AudioDisplayModelObsolete.h
    Displays/AudioDisplay/AudioDisplayModelObsolete.cpp
)

# ── Routing: Demux + Mux (obsolete) ─────────────────────────────────
set(ROUTING_NODES
    Routing/Demux/DemuxNodeObsolete.h
    Routing/Demux/DemuxNodeObsolete.cpp
    Routing/Mux/MuxNodeObsolete.h
    Routing/Mux/MuxNodeObsolete.cpp
    # GenericQDevIoConnectorObsolete is owned by node_editor_ide's
    # NodeEditorLibrary, but this plugin is configured *before* node_editor_ide
    # (root CMakeLists.txt), so that target does not exist yet and cannot be
    # linked here. Compile the translation unit directly instead, following the
    # same pattern used for TexturePool.cpp / ComputePool.cpp below.
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor_ide/BuiltInNodes/Library/connectors/GenericQDevIoConnectorObsolete.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor_ide/BuiltInNodes/Library/connectors/NodeDataModelToQIODeviceConnectorObsolete.cpp
    # QDevIoDisplayModelObsolete is needed by AudioDisplayModelObsolete and
    # AudioNodeQdevIoConnectorObsolete in this plugin. Compile it directly.
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor_ide/BuiltInNodes/Library/display/QDevIoDisplayModelObsolete.cpp
    # XYSeriesIODeviceObsolete is needed by AudioDisplayModelObsolete and
    # AudioNodeQdevIoConnectorObsolete. Compile it directly.
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor_ide/BuiltInNodes/Library/display/XYSeriesIODeviceObsolete.cpp
    # QDevioDisplayModelUiObsolete is needed by AudioDisplayModelObsolete. Compile it directly.
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor_ide/BuiltInNodes/Library/display/QDevioDisplayModelUiObsolete.cpp
    # EventThreadPullObsolete and AudioFrameDecoder are needed by
    # AudioDisplayModelObsolete and AudioNodeQdevIoConnectorObsolete.
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor_ide/BuiltInNodes/Library/threading/EventThreadPullObsolete.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor_ide/BuiltInNodes/Library/decoders/AudioFrameDecoder.cpp
)

# ── Shared types ────────────────────────────────────────────────────
set(SHARED_TYPES
    shared/IStoppable.h
    shared/IStartable.h
    shared/NetworkFrame.h
)

# ── Built-in nodes (NumberSource, NumberDisplay, Modulo, ArithmeticLogic) ──
set(BUILTIN_SOURCE_NODES
    Sources/NumberSource/NumberSourceDataModel.h
    Sources/NumberSource/NumberSourceDataModel.cpp
)

set(BUILTIN_DISPLAY_NODES
    Displays/NumberDisplay/NumberDisplayDataModel.h
    Displays/NumberDisplay/NumberDisplayDataModel.cpp
)

set(BUILTIN_OPERATOR_NODES
    Operators/ModuloModel.h
    Operators/ModuloModel.cpp
    Operators/ArithmeticLogic/ExprParser.h
    Operators/ArithmeticLogic/ExprParser.cpp
    Operators/ArithmeticLogic/ArithmeticLogicModel.h
    Operators/ArithmeticLogic/ArithmeticLogicModel.cpp
)

# ── Embedded node widgets ───────────────────────────────────────────
# REQ-SW-PL-051: "headless = no canvas, NOT no widgets" (user decision,
# 2026-09-25). The node MODELS above live in this plugin and reference their
# widget classes directly, so the widget translation units must be compiled
# into this plugin as well — otherwise the plugin keeps undefined references
# and dlopen() fails, which silently disables every core node type.
# The GUI plugin keeps its own copies for the editor-side embedding.
set(EMBEDDED_WIDGETS
    Sources/Video/VideoGLBlitWidget.h
    Sources/Video/VideoGLBlitWidget.cpp
)

# ── Use the Daqster plugin template ──────────────────────────────────
create_plugin(DemoNodeEditorNodesCore
    SOURCES
        ${PLUGIN_SOURCES}
        ${AUDIO_SOURCE_NODES}
        ${LLAMA_SOURCE_NODES}
        ${VIDEO_NODES}
        ${VIDEO_OPENCV_SOURCES}
        ${PLUTOSDR_SOURCES}
        ${SYSTEM_MONITOR_SOURCES}
        ${GAMEPAD_SOURCES}
        ${FILE_IO_NODES}
        ${NETWORK_NODES}
        ${GPU_MONITOR_SOURCES}
        ${JACK_DETECT_SOURCES}
        ${PCAP_SOURCES}
        ${DAQ_DISPLAY_NODES}
        ${ROUTING_NODES}
        ${SHARED_TYPES}
        ${BUILTIN_SOURCE_NODES}
        ${BUILTIN_DISPLAY_NODES}
        ${BUILTIN_OPERATOR_NODES}
        ${EMBEDDED_WIDGETS}
    REQUIRES_LIBRARIES
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        Qt${QT_VERSION_MAJOR}::Charts
        Qt${QT_VERSION_MAJOR}::Multimedia
        Qt${QT_VERSION_MAJOR}::Network
        Qt${QT_VERSION_MAJOR}::OpenGL
        Qt${QT_VERSION_MAJOR}::Widgets
        ${DEMO_CORE_GUI_LIBS}
        QtNodes
        FrameworkCore
    COMPILE_DEFINITIONS
        ${VIDEO_OPENCV_DEFINITIONS}
        ${PLUTOSDR_DEFINITIONS}
        ${SYSTEM_MONITOR_DEFINITIONS}
        ${GAMEPAD_DEFINITIONS}
        ${GPU_MONITOR_DEFINITIONS}
        ${JACK_DETECT_DEFINITIONS}
        ${PCAP_DEFINITIONS}
    INCLUDE_DIRECTORIES
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${DAQSTER_SOURCE_DIR}/src/plugins/common
        ${DAQSTER_SOURCE_DIR}/src/frame_work/base/src/include
        ${DAQSTER_SOURCE_DIR}/src/frame_work/base/src/logging
        ${CMAKE_CURRENT_SOURCE_DIR}/Sources/AudioSource
        ${CMAKE_CURRENT_SOURCE_DIR}/Sources/LLamaSource
        ${CMAKE_CURRENT_SOURCE_DIR}/Sources/Video
        ${CMAKE_CURRENT_SOURCE_DIR}/Sources/SystemMonitor
        ${CMAKE_CURRENT_SOURCE_DIR}/Sources/Gamepad
        ${CMAKE_CURRENT_SOURCE_DIR}/Sources/Pcap
        ${CMAKE_CURRENT_SOURCE_DIR}/Sources/NumberSource
        ${CMAKE_CURRENT_SOURCE_DIR}/Displays/NumberDisplay
        ${CMAKE_CURRENT_SOURCE_DIR}/Operators
        ${CMAKE_CURRENT_SOURCE_DIR}/Operators/ArithmeticLogic
        ${CMAKE_CURRENT_SOURCE_DIR}/shared
        ${CMAKE_CURRENT_SOURCE_DIR}/Displays/DaqDisplay
        ${VIDEO_OPENCV_INCLUDE_DIRS}
        ${PLUTOSDR_INCLUDE_DIRS}
        ${GPU_MONITOR_INCLUDE_DIRS}
        ${PCAP_INCLUDE_DIRS}
    LINK_LIBRARIES
        ${VIDEO_OPENCV_LINK_LIBRARIES}
        ${PLUTOSDR_LINK_LIBRARIES}
        ${GPU_MONITOR_LINK_LIBRARIES}
        ${PCAP_LINK_LIBRARIES}
)

# ── Output name must contain "plugin" substring ──────────────────────
if(TARGET DemoNodeEditorNodesCore)
    set_target_properties(DemoNodeEditorNodesCore PROPERTIES
        OUTPUT_NAME "DemoNodeEditorNodesCorePlugin"
    )
    # Windows: export all symbols from the plugin DLL
    if(WIN32)
        set_target_properties(DemoNodeEditorNodesCore PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
    endif()

    # demo_nodeditor_nodes_core_export.h is generated here and exposed publicly
    # so that DemoNodeEditorNodesGui receives DEMO_NODEDITOR_NODES_CORE_EXPORT
    # through its link to this plugin. WINDOWS_EXPORT_ALL_SYMBOLS alone does not
    # export the core classes that the GUI plugin references at link time.
    generate_export_header(DemoNodeEditorNodesCore
        BASE_NAME demo_nodeditor_nodes_core
        EXPORT_FILE_NAME ${CMAKE_CURRENT_BINARY_DIR}/demo_nodeditor_nodes_core_export.h
    )
    target_include_directories(DemoNodeEditorNodesCore
        PUBLIC
            ${CMAKE_CURRENT_BINARY_DIR}
    )

    if(TARGET NodeEditorLibrary)
        add_dependencies(DemoNodeEditorNodesCore NodeEditorLibrary)
    endif()
endif()