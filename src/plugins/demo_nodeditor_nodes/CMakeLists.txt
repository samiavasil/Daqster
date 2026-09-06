cmake_minimum_required(VERSION 3.16)

project(demo_nodeditor_nodes VERSION 0.3.2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# ── Plugin JSON metadata (generated from template) ──────────────────
# REQ-SW-PL-035: the plugin metadata JSON is generated from a .json.in
# template with @DAQSTER_VERSION@ substituted (single source of truth:
# root VERSION file). Must run BEFORE create_plugin() so the JSON exists
# when moc resolves Q_PLUGIN_METADATA FILE.
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/DemoNodeEditorNodesInterface.json.in"
    "${CMAKE_CURRENT_SOURCE_DIR}/DemoNodeEditorNodesInterface.json"
    @ONLY
)

# ── Plugin infrastructure (root level) ──────────────────────────────
set(PLUGIN_SOURCES
    DemoNodeEditorNodesInterface.cpp
    DemoNodeEditorNodesInterface.h
    DemoNodeEditorNodesObject.cpp
    DemoNodeEditorNodesObject.h
)

# ── Sources: AudioSource ────────────────────────────────────────────
# REQ-SW-PL-024: the SampledData AudioSourceDataModel (new, takes the
# "AudioSource" name, worker-thread capture via MicCaptureWorker) + shared
# UI; the old QDevIO mic was renamed to _obsolete and keeps working
# together with its helpers.
set(AUDIO_SOURCE_NODES
    Sources/AudioSource/AudioSourceDataModel.h
    Sources/AudioSource/AudioSourceDataModel.cpp
    Sources/AudioSource/MicCaptureWorker.h
    Sources/AudioSource/MicCaptureWorker.cpp
    Sources/AudioSource/AudioSourceDataModelObsolete.h
    Sources/AudioSource/AudioSourceDataModelObsolete.cpp
    Sources/AudioSource/AudioSourceDataModelUI.h
    Sources/AudioSource/AudioSourceDataModelUI.cpp
    Sources/AudioSource/AudioSourceDataModelUI.ui
    Sources/AudioSource/AudioSourceConfig.h
    Sources/AudioSource/AudioSourceConfig.cpp
    Sources/AudioSource/AudioSourceConfig.ui
    Sources/AudioSource/AudioWorkerObsolete.h
    Sources/AudioSource/AudioWorkerObsolete.cpp
    Sources/AudioSource/AudioNodeQdevIoConnectorObsolete.h
    Sources/AudioSource/AudioNodeQdevIoConnectorObsolete.cpp
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

# ── Video nodes (VideoFrameData / "video-frame" flow) ───────────────
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
    Sources/Video/VideoGLBlitWidget.h
    Sources/Video/VideoGLBlitWidget.cpp
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
    # Texture pool for GPU effect outputs (REQ-SW-PL-032 Issue #7) — lives in
    # the shared GL helpers next to VideoGLContextManager.
    ${DAQSTER_SOURCE_DIR}/src/plugins/common/GL/TexturePool.cpp
    # Common compute thread pool (REQ-SW-PL-039) — shared by all nodes, not
    # per-node; per-key latest-wins submission + per-key serialization.
    ${DAQSTER_SOURCE_DIR}/src/plugins/common/Threading/ComputePool.cpp
)

# ── Optional OpenCV transforms (compile-time auto-detect) ───────────
# REQ-SW-PL-019: when OpenCV is found, OpenCVTransforms.cpp is compiled and
# HAVE_OPENCV is defined, exposing 3 additional video transform operations
# (GaussianBlur, Canny, Threshold). Without OpenCV the plugin builds normally
# with the 8 base QImage operations.
option(DAQSTER_USE_OPENCV "Enable OpenCV-based video transform operations" ON)
find_package(OpenCV QUIET)
set(VIDEO_OPENCV_SOURCES)
set(VIDEO_OPENCV_DEFINITIONS)
set(VIDEO_OPENCV_INCLUDE_DIRS)
set(VIDEO_OPENCV_LINK_LIBRARIES)
if(DAQSTER_USE_OPENCV AND OpenCV_FOUND)
    message(STATUS "demo_nodeditor_nodes: OpenCV ${OpenCV_VERSION} found — enabling OpenCV video transform operations")
    list(APPEND VIDEO_OPENCV_SOURCES Sources/Video/OpenCVTransforms.cpp)
    list(APPEND VIDEO_OPENCV_DEFINITIONS HAVE_OPENCV)
    list(APPEND VIDEO_OPENCV_INCLUDE_DIRS ${OpenCV_INCLUDE_DIRS})
    list(APPEND VIDEO_OPENCV_LINK_LIBRARIES ${OpenCV_LIBS})
else()
    message(STATUS "demo_nodeditor_nodes: OpenCV not found or disabled — using the 8 base video transform operations")
endif()

# ── Optional libiio PlutoSDR node (compile-time auto-detect) ─────────
# REQ-SW-PL-040: when libiio is found, the PlutoSdr sources are compiled and
# HAVE_LIBIIO is defined, exposing the PlutoSDR RX DAQ node (IQ streaming via
# libiio, SampledData domain="iq"). Without libiio the plugin builds normally
# without the node (model of the OpenCV block above) — keeps Qt5/Qt6 builds
# green on machines without libiio (CI, Windows).
set(PLUTOSDR_SOURCES)
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
    message(STATUS "demo_nodeditor_nodes: libiio ${IIO_VERSION} found — enabling PlutoSDR RX DAQ node")
    list(APPEND PLUTOSDR_SOURCES
        Sources/PlutoSdr/PlutoSdrEngine.h
        Sources/PlutoSdr/PlutoSdrEngine.cpp
        Sources/PlutoSdr/PlutoSdrModel.h
        Sources/PlutoSdr/PlutoSdrModel.cpp
        Sources/PlutoSdr/PlutoSdrWidget.h
        Sources/PlutoSdr/PlutoSdrWidget.cpp
    )
    list(APPEND PLUTOSDR_DEFINITIONS HAVE_LIBIIO)
    list(APPEND PLUTOSDR_INCLUDE_DIRS ${IIO_INCLUDE_DIRS})
    if(TARGET IIO::IIO)
        list(APPEND PLUTOSDR_LINK_LIBRARIES IIO::IIO)
    else()
        list(APPEND PLUTOSDR_LINK_LIBRARIES ${IIO_LIBRARIES})
    endif()
else()
    message(STATUS "demo_nodeditor_nodes: libiio not found — PlutoSDR RX DAQ node disabled")
endif()

# ── Optional System Monitor source node (Linux /proc + /sys) ─────────
# REQ-SW-PL-041: reads Linux system telemetry (/proc/stat, /proc/meminfo,
# /sys/class/hwmon, /proc/net/dev) and emits it as SampledData domain="system".
# Linux-only v1 — guarded by if(NOT WIN32) so Windows builds pass without the
# node (Windows PDH/WMI support is a future requirement).
set(SYSTEM_MONITOR_SOURCES)
set(SYSTEM_MONITOR_DEFINITIONS)
if(NOT WIN32)
    message(STATUS "demo_nodeditor_nodes: System Monitor source node enabled (Linux /proc + /sys)")
    list(APPEND SYSTEM_MONITOR_SOURCES
        Sources/SystemMonitor/SystemMonitorEngine.h
        Sources/SystemMonitor/SystemMonitorEngine.cpp
        Sources/SystemMonitor/SystemMonitorModel.h
        Sources/SystemMonitor/SystemMonitorModel.cpp
        Sources/SystemMonitor/SystemMonitorWidget.h
        Sources/SystemMonitor/SystemMonitorWidget.cpp
    )
    list(APPEND SYSTEM_MONITOR_DEFINITIONS HAVE_SYSTEM_MONITOR)
else()
    message(STATUS "demo_nodeditor_nodes: System Monitor source node disabled (Windows)")
endif()

# ── Optional Gamepad source node (Linux joystick API) ────────────────
# REQ-SW-PL-042: reads axes + buttons from a USB gamepad via the Linux
# joystick API (/dev/input/js0) and emits them as SampledData domain="gamepad".
# Linux-only v1 — guarded by if(NOT WIN32) so Windows builds pass without the
# node (Windows XInput/DirectInput support is a future requirement).
set(GAMEPAD_SOURCES)
set(GAMEPAD_DEFINITIONS)
if(NOT WIN32)
    message(STATUS "demo_nodeditor_nodes: Gamepad source node enabled (Linux joystick API)")
    list(APPEND GAMEPAD_SOURCES
        Sources/Gamepad/GamepadEngine.h
        Sources/Gamepad/GamepadEngine.cpp
        Sources/Gamepad/GamepadModel.h
        Sources/Gamepad/GamepadModel.cpp
        Sources/Gamepad/GamepadWidget.h
        Sources/Gamepad/GamepadWidget.cpp
    )
    list(APPEND GAMEPAD_DEFINITIONS HAVE_GAMEPAD)
else()
    message(STATUS "demo_nodeditor_nodes: Gamepad source node disabled (Windows)")
endif()

# ── File Record sink + File Playback source (REQ-SW-PL-043) ──────────
# File I/O is cross-platform — no platform guard needed. The pair records
# SampledData streams to disk as raw bytes + JSON sidecar and plays them back
# at the recorded sample rate.
set(FILE_IO_NODES
    Sinks/FileRecord/FileRecordModel.h
    Sinks/FileRecord/FileRecordModel.cpp
    Sinks/FileRecord/FileRecordWidget.h
    Sinks/FileRecord/FileRecordWidget.cpp
    Sources/FilePlayback/FilePlaybackModel.h
    Sources/FilePlayback/FilePlaybackModel.cpp
    Sources/FilePlayback/FilePlaybackWidget.h
    Sources/FilePlayback/FilePlaybackWidget.cpp
)

# ── Network Source + Sink (REQ-SW-PL-044) ─────────────────────────────
# UDP/TCP transport of SampledData streams over the network. Cross-platform —
# Qt Network module is part of Qt5/Qt6, no external deps, no platform guard.
set(NETWORK_NODES
    Sources/NetworkSource/NetworkSourceModel.h
    Sources/NetworkSource/NetworkSourceModel.cpp
    Sources/NetworkSource/NetworkSourceWidget.h
    Sources/NetworkSource/NetworkSourceWidget.cpp
    Sinks/NetworkSink/NetworkSinkModel.h
    Sinks/NetworkSink/NetworkSinkModel.cpp
    Sinks/NetworkSink/NetworkSinkWidget.h
    Sinks/NetworkSink/NetworkSinkWidget.cpp
    shared/NetworkFrame.h
)

# ── Optional NVML GPU Monitor source (compile-time auto-detect) ──────
# REQ-SW-PL-045: when NVML (NVIDIA Management Library) is found, the
# GpuMonitor source node is compiled and HAVE_NVML is defined, exposing a
# SampledData (domain="gpu") source that reads GPU telemetry. Without NVML the
# plugin builds normally without the node (same optional-dependency model as
# OpenCV above).
set(GPU_MONITOR_SOURCES)
set(GPU_MONITOR_DEFINITIONS)
set(GPU_MONITOR_INCLUDE_DIRS)
set(GPU_MONITOR_LINK_LIBRARIES)
find_library(NVML_LIBRARY NAMES nvidia-ml)
find_path(NVML_INCLUDE_DIR NAMES nvml.h)
if(NVML_LIBRARY AND NVML_INCLUDE_DIR)
    message(STATUS "demo_nodeditor_nodes: NVML found — enabling GPU Monitor source node")
    list(APPEND GPU_MONITOR_SOURCES
        Sources/GpuMonitor/GpuMonitorEngine.cpp
        Sources/GpuMonitor/GpuMonitorModel.cpp
        Sources/GpuMonitor/GpuMonitorWidget.cpp
    )
    list(APPEND GPU_MONITOR_DEFINITIONS HAVE_NVML)
    list(APPEND GPU_MONITOR_INCLUDE_DIRS ${NVML_INCLUDE_DIR})
    list(APPEND GPU_MONITOR_LINK_LIBRARIES ${NVML_LIBRARY})
else()
    message(STATUS "demo_nodeditor_nodes: NVML not found — GPU Monitor source node disabled")
endif()
# ── Displays: AudioDisplay (QDevIO, obsolete — REQ-SW-PL-023 §7) ─────
set(AUDIO_DISPLAY_NODES
    Displays/AudioDisplay/AudioDisplayModelObsolete.h
    Displays/AudioDisplay/AudioDisplayModelObsolete.cpp
)

# ── Displays: DAQ Display (DataPlot, REQ-SW-PL-022) ─────────────────
set(DAQ_DISPLAY_NODES
    Displays/DaqDisplay/DaqDisplayNode.h
    Displays/DaqDisplay/DaqDisplayNode.cpp
)

# ── Displays: GenericDisplay (legacy alias of DAQ Display) ──────────
set(GENERIC_DISPLAY_NODES
    Displays/GenericDisplay/GenericDisplayNode.h
    Displays/GenericDisplay/GenericDisplayNode.cpp
)

# ── Routing: Demux + Mux (QDevIO, obsolete — REQ-SW-PL-023 §7) ──────
set(ROUTING_NODES
    Routing/Demux/DemuxNodeObsolete.h
    Routing/Demux/DemuxNodeObsolete.cpp
    Routing/Mux/MuxNodeObsolete.h
    Routing/Mux/MuxNodeObsolete.cpp
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
        ${VIDEO_NODES}
        ${VIDEO_OPENCV_SOURCES}
${PLUTOSDR_SOURCES}
        ${SYSTEM_MONITOR_SOURCES}
        ${GAMEPAD_SOURCES}
        ${FILE_IO_NODES}
        ${NETWORK_NODES}
        ${GPU_MONITOR_SOURCES}
        ${GENERIC_DISPLAY_NODES}
        ${AUDIO_DISPLAY_NODES}
        ${DAQ_DISPLAY_NODES}
        ${ROUTING_NODES}
        ${SHARED_TYPES}
    REQUIRES_LIBRARIES
        NodeEditorLibrary
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        Qt${QT_VERSION_MAJOR}::Charts
         Qt${QT_VERSION_MAJOR}::Multimedia
         Qt${QT_VERSION_MAJOR}::MultimediaWidgets
         Qt${QT_VERSION_MAJOR}::Network
        Qt${QT_VERSION_MAJOR}::OpenGL
        QtNodes
        frame_work
    COMPILE_DEFINITIONS
        ${VIDEO_OPENCV_DEFINITIONS}
        ${PLUTOSDR_DEFINITIONS}
        ${SYSTEM_MONITOR_DEFINITIONS}
        ${GAMEPAD_DEFINITIONS}
        ${GPU_MONITOR_DEFINITIONS}
    INCLUDE_DIRECTORIES
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${DAQSTER_SOURCE_DIR}/src/plugins/common
        ${CMAKE_CURRENT_SOURCE_DIR}/Sources/AudioSource
        ${CMAKE_CURRENT_SOURCE_DIR}/Sources/LLamaSource
        ${CMAKE_CURRENT_SOURCE_DIR}/Sources/Video
        ${CMAKE_CURRENT_SOURCE_DIR}/Sources/SystemMonitor
        ${CMAKE_CURRENT_SOURCE_DIR}/Sources/Gamepad
        ${CMAKE_CURRENT_SOURCE_DIR}/shared
        ${CMAKE_CURRENT_SOURCE_DIR}/Displays/AudioDisplay
        ${CMAKE_CURRENT_SOURCE_DIR}/Displays/DaqDisplay
        ${VIDEO_OPENCV_INCLUDE_DIRS}
        ${PLUTOSDR_INCLUDE_DIRS}
        ${GPU_MONITOR_INCLUDE_DIRS}
    LINK_LIBRARIES
        ${VIDEO_OPENCV_LINK_LIBRARIES}
        ${PLUTOSDR_LINK_LIBRARIES}
        ${GPU_MONITOR_LINK_LIBRARIES}
)

# ── Output name must contain "plugin" substring ──────────────────────
if(TARGET DemoNodeEditorNodes)
    set_target_properties(DemoNodeEditorNodes PROPERTIES
        OUTPUT_NAME "DemoNodeEditorNodesPlugin"
    )
    # Explicit ordering guarantee: DemoNodeEditorNodesPlugin.dll links against
    # NodeEditorLibrary's import library on Windows (same LNK1181 guard as
    # NodeEditorIde). The link already goes through the target name; this
    # add_dependencies() is a belt-and-braces order-only edge.
    if(TARGET NodeEditorLibrary)
        add_dependencies(DemoNodeEditorNodes NodeEditorLibrary)
    endif()
endif()
