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
    INCLUDE_DIRECTORIES
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${DAQSTER_SOURCE_DIR}/src/plugins/common
        ${CMAKE_CURRENT_SOURCE_DIR}/Sources/AudioSource
        ${CMAKE_CURRENT_SOURCE_DIR}/Sources/LLamaSource
        ${CMAKE_CURRENT_SOURCE_DIR}/Sources/Video
        ${CMAKE_CURRENT_SOURCE_DIR}/Displays/AudioDisplay
        ${CMAKE_CURRENT_SOURCE_DIR}/Displays/DaqDisplay
        ${VIDEO_OPENCV_INCLUDE_DIRS}
    LINK_LIBRARIES
        ${VIDEO_OPENCV_LINK_LIBRARIES}
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
