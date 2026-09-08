cmake_minimum_required(VERSION 3.16)

project(demo_nodeditor_nodes_core VERSION 0.3.2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

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
    Sources/AudioSource/AudioSourceConfig.h
    Sources/AudioSource/AudioSourceConfig.cpp
    Sources/AudioSource/AudioComboModel.h
    Sources/AudioSource/AudioComboModel.cpp
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
    ${DAQSTER_SOURCE_DIR}/src/gui/library/GL/TexturePool.cpp
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
    message(STATUS "demo_nodeditor_nodes_core: libiio ${IIO_VERSION} found — enabling PlutoSDR RX DAQ node")
    list(APPEND PLUTOSDR_SOURCES
        Sources/PlutoSdr/PlutoSdrEngine.h
        Sources/PlutoSdr/PlutoSdrEngine.cpp
        Sources/PlutoSdr/PlutoSdrModel.h
        Sources/PlutoSdr/PlutoSdrModel.cpp
    )
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
set(SYSTEM_MONITOR_SOURCES)
set(SYSTEM_MONITOR_DEFINITIONS)
if(NOT WIN32)
    message(STATUS "demo_nodeditor_nodes_core: System Monitor source node enabled (Linux /proc + /sys)")
    list(APPEND SYSTEM_MONITOR_SOURCES
        Sources/SystemMonitor/SystemMonitorEngine.h
        Sources/SystemMonitor/SystemMonitorEngine.cpp
        Sources/SystemMonitor/SystemMonitorModel.h
        Sources/SystemMonitor/SystemMonitorModel.cpp
    )
    list(APPEND SYSTEM_MONITOR_DEFINITIONS HAVE_SYSTEM_MONITOR)
else()
    message(STATUS "demo_nodeditor_nodes_core: System Monitor source node disabled (Windows)")
endif()

# ── Optional Gamepad source node ────────────────────────────────────
set(GAMEPAD_SOURCES)
set(GAMEPAD_DEFINITIONS)
if(NOT WIN32)
    message(STATUS "demo_nodeditor_nodes_core: Gamepad source node enabled (Linux joystick API)")
    list(APPEND GAMEPAD_SOURCES
        Sources/Gamepad/GamepadEngine.h
        Sources/Gamepad/GamepadEngine.cpp
        Sources/Gamepad/GamepadModel.h
        Sources/Gamepad/GamepadModel.cpp
    )
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
set(GPU_MONITOR_SOURCES)
set(GPU_MONITOR_DEFINITIONS)
set(GPU_MONITOR_INCLUDE_DIRS)
set(GPU_MONITOR_LINK_LIBRARIES)
find_library(NVML_LIBRARY NAMES nvidia-ml)
find_path(NVML_INCLUDE_DIR NAMES nvml.h)
if(NVML_LIBRARY AND NVML_INCLUDE_DIR)
    message(STATUS "demo_nodeditor_nodes_core: NVML found — enabling GPU Monitor source node")
    list(APPEND GPU_MONITOR_SOURCES
        Sources/GpuMonitor/GpuMonitorEngine.cpp
        Sources/GpuMonitor/GpuMonitorModel.cpp
    )
    list(APPEND GPU_MONITOR_DEFINITIONS HAVE_NVML)
    list(APPEND GPU_MONITOR_INCLUDE_DIRS ${NVML_INCLUDE_DIR})
    list(APPEND GPU_MONITOR_LINK_LIBRARIES ${NVML_LIBRARY})
else()
    message(STATUS "demo_nodeditor_nodes_core: NVML not found — GPU Monitor source node disabled")
endif()

# ── Optional Jack-detect source ─────────────────────────────────────
set(JACK_DETECT_SOURCES)
set(JACK_DETECT_DEFINITIONS)
if(NOT WIN32)
    message(STATUS "demo_nodeditor_nodes_core: Jack-detect source node enabled (Linux HDA /proc/asound)")
    list(APPEND JACK_DETECT_SOURCES
        Sources/JackDetect/JackDetectEngine.cpp
        Sources/JackDetect/JackDetectModel.cpp
    )
    list(APPEND JACK_DETECT_DEFINITIONS HAVE_JACK_DETECT)
else()
    message(STATUS "demo_nodeditor_nodes_core: Jack-detect source node disabled (Windows)")
endif()

# ── Optional libpcap Packet Capture source ──────────────────────────
set(PCAP_SOURCES)
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
    list(APPEND PCAP_SOURCES
        Sources/Pcap/PcapEngine.cpp
        Sources/Pcap/PcapModel.cpp
    )
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
)

# ── Routing: Demux + Mux (obsolete) ─────────────────────────────────
set(ROUTING_NODES
    Routing/Demux/DemuxNodeObsolete.h
    Routing/Demux/DemuxNodeObsolete.cpp
    Routing/Mux/MuxNodeObsolete.h
    Routing/Mux/MuxNodeObsolete.cpp
)

# ── Shared types ────────────────────────────────────────────────────
set(SHARED_TYPES
    shared/IStoppable.h
    shared/IStartable.h
    shared/NetworkFrame.h
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
    REQUIRES_LIBRARIES
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        Qt${QT_VERSION_MAJOR}::Charts
        Qt${QT_VERSION_MAJOR}::Multimedia
        Qt${QT_VERSION_MAJOR}::Network
        Qt${QT_VERSION_MAJOR}::OpenGL
        QtNodes
        DaqsterCore
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
        ${DAQSTER_SOURCE_DIR}/src/core
        ${DAQSTER_SOURCE_DIR}/src/gui/library
        ${CMAKE_CURRENT_SOURCE_DIR}/Sources/AudioSource
        ${CMAKE_CURRENT_SOURCE_DIR}/Sources/LLamaSource
        ${CMAKE_CURRENT_SOURCE_DIR}/Sources/Video
        ${CMAKE_CURRENT_SOURCE_DIR}/Sources/SystemMonitor
        ${CMAKE_CURRENT_SOURCE_DIR}/Sources/Gamepad
        ${CMAKE_CURRENT_SOURCE_DIR}/Sources/Pcap
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
    if(TARGET NodeEditorLibrary)
        add_dependencies(DemoNodeEditorNodesCore NodeEditorLibrary)
    endif()
endif()