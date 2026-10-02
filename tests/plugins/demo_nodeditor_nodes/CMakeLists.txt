# Unit tests for the demo_nodeditor_nodes video plugin logic.
#
# The code under test (VideoTransformOps + StreamUrlValidator) is QtCore/QtGui
# only - no Multimedia, no QtNodes, no frame_work - so it is compiled directly
# into the test binary instead of linking the plugin .so. The tests run
# headless (QCoreApplication, no widgets).
#
# The test classes (TestVideoTransformOps/TestStreamUrlValidator/
# SampledDataTest/SampledStreamDescriptorTest/FftUtilTest/
# AudioBufferToSampledTest/TestVideoPerfBadge) are declared in headers
# and share a single binary (demo_nodeditor_nodes_tests) whose main() lives in
# test_main.cpp. Each class is run through QTest::qExec there, mirroring
# QTEST_GUILESS_MAIN.
#
# OpenCV-backed operations (OpenCVTransforms.cpp) are deliberately NOT part of
# this target: the 8 base QImage operations in VideoTransformOps.cpp are the
# required coverage (REQ-SW-PL-018/019) and the tests must not require OpenCV.

find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Test Multimedia OpenGL REQUIRED)
if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    find_package(Qt${QT_VERSION_MAJOR} COMPONENTS MultimediaWidgets REQUIRED)
endif()

# The test classes declare Q_OBJECT in their headers - the root CMakeLists.txt
# already sets CMAKE_AUTOMOC ON globally; we repeat it here so this directory
# is self-contained when tests are built standalone.
set(CMAKE_AUTOMOC ON)

# The core model/engine headers include the plugin's generated export header
# (demo_nodeditor_nodes_core_export.h). These test binaries compile core sources
# directly instead of linking the plugin .so, so they need two things:
#   - the generated directory on their own include path (they do not inherit the
#     plugin's PUBLIC include directories);
#   - DEMO_NODEDITOR_NODES_CORE_STATIC_DEFINE, which turns the export macro into
#     nothing. Without it the header would ask for __declspec(dllimport) on
#     Windows while the definitions are compiled straight into the test binary
#     (LNK2001 / C2491).
set(DEMO_CORE_EXPORT_INCLUDE_DIR ${CMAKE_BINARY_DIR}/src/plugins/demo_nodeditor_nodes_core)
set(DEMO_CORE_EXPORT_DEFINITIONS DEMO_NODEDITOR_NODES_CORE_STATIC_DEFINE)

set(TEST_SOURCES
    test_main.cpp
    test_video_transform_ops.cpp test_video_transform_ops.h
    test_stream_url_validator.cpp test_stream_url_validator.h
    test_sampled_data.cpp test_sampled_data.h
    test_sampled_stream_descriptor.cpp test_sampled_stream_descriptor.h
    test_fft_util.cpp test_fft_util.h
    test_audio_buffer_to_sampled.cpp test_audio_buffer_to_sampled.h
    test_video_perf_badge.cpp test_video_perf_badge.h
    # plugin logic under test (compiled directly into the test binary):
    ${DAQSTER_SOURCE_DIR}/src/plugins/demo_nodeditor_nodes_core/Sources/Video/VideoTransformOps.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/demo_nodeditor_nodes_core/Sources/Video/StreamUrlValidator.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/demo_nodeditor_nodes_core/Sources/Video/VideoPerfBadge.cpp
)

add_executable(demo_nodeditor_nodes_tests ${TEST_SOURCES})

target_include_directories(demo_nodeditor_nodes_tests PRIVATE
    ${DAQSTER_SOURCE_DIR}/src/plugins/demo_nodeditor_nodes_core/Sources/Video
    ${DAQSTER_SOURCE_DIR}/src/plugins/common
    ${DAQSTER_SOURCE_DIR}/src/plugins/external_libs/nodeeditor/include
    ${DEMO_CORE_EXPORT_INCLUDE_DIR}
)
target_compile_definitions(demo_nodeditor_nodes_tests PRIVATE ${DEMO_CORE_EXPORT_DEFINITIONS})

target_link_libraries(demo_nodeditor_nodes_tests PRIVATE
    Qt${QT_VERSION_MAJOR}::Test
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    # AudioBufferToSampled.h uses QAudioFormat/QAudioBuffer (QtMultimedia):
    Qt${QT_VERSION_MAJOR}::Multimedia
    # SampledData derives from QtNodes::NodeData (REQ-SW-PL-022): the QtNodes
    # target propagates the NODE_EDITOR_SHARED compile definition (Export.hpp
    # requires it) and supplies the NodeData base-class symbols.
    QtNodes
)

add_test(NAME demo_nodeditor_nodes_tests COMMAND $<TARGET_FILE:demo_nodeditor_nodes_tests>)

# ── DaqDisplayNode save/restore round-trip GUI tests ─────────────────────────
#
# DaqDisplayNode requires QApplication (QtCharts widgets) and the offscreen
# platform plugin. The node source is compiled directly into the test binary
# (same pattern as requirements_manager_gui_tests). QTEST_MAIN in
# test_daq_display_node.cpp provides the QApplication main.
#
# The QtChartsCompat.h header is in node_editor_ide/BuiltInNodes/Library/display/.
# DaqDisplayNode.cpp transitively depends on FftUtil.h + SampledData.h (both
# header-only, in src/plugins/common/) and on SampledStreamDescriptor.h (in
# NodeDataTypes/).
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Charts REQUIRED)

set(DISPLAY_TEST_SOURCES
    test_daq_display_node.cpp
    test_daq_display_node.h
    # plugin logic under test (compiled directly into the test binary):
    ${DAQSTER_SOURCE_DIR}/src/plugins/demo_nodeditor_nodes_core/Displays/DaqDisplay/DaqDisplayNode.cpp
    # GenericDisplayNode is a thin DaqDisplayNode subclass (name/caption only) —
    # compiled so the alias behavior + registry resolution tests link standalone.
    ${DAQSTER_SOURCE_DIR}/src/plugins/demo_nodeditor_nodes_core/Displays/GenericDisplay/GenericDisplayNode.cpp
    # DaqDisplayNode now submits compute to the shared ComputePool
    # (REQ-SW-PL-039) — compile the pool source so the test binary links
    # standalone.
    ${DAQSTER_SOURCE_DIR}/src/plugins/common/Threading/ComputePool.cpp
)

add_executable(demo_nodeditor_display_tests ${DISPLAY_TEST_SOURCES})

target_include_directories(demo_nodeditor_display_tests PRIVATE
    ${DAQSTER_SOURCE_DIR}/src/plugins/common
    ${DAQSTER_SOURCE_DIR}/src/plugins/common/NodeDataTypes
    ${DAQSTER_SOURCE_DIR}/src/plugins/demo_nodeditor_nodes_core
    ${DAQSTER_SOURCE_DIR}/src/plugins/demo_nodeditor_nodes_core/Displays/DaqDisplay
    ${DAQSTER_SOURCE_DIR}/src/plugins/demo_nodeditor_nodes_core/Displays/GenericDisplay
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor_ide/BuiltInNodes/Library/display
    ${DAQSTER_SOURCE_DIR}/src/plugins/external_libs/nodeeditor/include
    ${DEMO_CORE_EXPORT_INCLUDE_DIR}
)
target_compile_definitions(demo_nodeditor_display_tests PRIVATE ${DEMO_CORE_EXPORT_DEFINITIONS})

target_link_libraries(demo_nodeditor_display_tests PRIVATE
    Qt${QT_VERSION_MAJOR}::Test
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Charts
    QtNodes
    NodeEditorLibrary
)

add_test(NAME demo_nodeditor_display_tests
         COMMAND demo_nodeditor_display_tests)
set_tests_properties(demo_nodeditor_display_tests PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# ── VideoFrameData value-semantic tests (REQ-SW-PL-020, AC 6) ────────────────
#
# QTEST_GUILESS_MAIN provides QCoreApplication — QImage operations do not
# need a widget-backed event loop. On Qt5 only the stub path is compiled;
# Qt6 brings QVideoFrame-based round-trip tests.
set(VIDEO_FRAME_TEST_SOURCES
    test_video_frame_data.cpp
    test_video_frame_data.h
    # VideoFrameData::releaseTextures() now returns pooled textures to the
    # global TexturePool (REQ-SW-PL-038) — compile the pool source so the test
    # binary links standalone.
    ${DAQSTER_SOURCE_DIR}/src/plugins/common/GL/TexturePool.cpp
)

add_executable(demo_nodeditor_videoframe_tests ${VIDEO_FRAME_TEST_SOURCES})

target_include_directories(demo_nodeditor_videoframe_tests PRIVATE
    ${DAQSTER_SOURCE_DIR}/src/plugins/common
    ${DAQSTER_SOURCE_DIR}/src/plugins/external_libs/nodeeditor/include
)

target_link_libraries(demo_nodeditor_videoframe_tests PRIVATE
    Qt${QT_VERSION_MAJOR}::Test
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Multimedia
    QtNodes
    NodeEditorLibrary
)

add_test(NAME demo_nodeditor_videoframe_tests
         COMMAND demo_nodeditor_videoframe_tests)
set_tests_properties(demo_nodeditor_videoframe_tests PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# ── VideoOutputNode behavioural tests (REQ-SW-PL-020, AC 6) ──────────────────
#
# QTEST_MAIN provides QApplication; the binary runs headless via the offscreen
# platform plugin. VideoOutputNode.cpp is compiled directly into the test
# binary so we can exercise input/output connection callbacks without linking
# the plugin .so.
set(VIDEO_OUTPUT_TEST_SOURCES
    test_video_output_node.cpp
    test_video_output_node.h
    ${DAQSTER_SOURCE_DIR}/src/plugins/demo_nodeditor_nodes_core/Sources/Video/VideoOutputNode.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/demo_nodeditor_nodes_core/Sources/Video/VideoPerfBadge.cpp
    # VideoOutputNode references VideoGLBlitWidget (DAQSTER_GL_BLIT display path);
    # compile the widget source so the test binary links standalone:
    ${DAQSTER_SOURCE_DIR}/src/plugins/demo_nodeditor_nodes_core/Sources/Video/VideoGLBlitWidget.cpp
    # Embedded effects (REQ-SW-PL-034): VideoOutputNode now uses
    # VideoEffectOps::allSpecs() + VideoEffectGLProcessor::processTexture().
    # VideoEffectOps.cpp delegates to VideoTransformOps.cpp for the CPU apply
    # functions; VideoEffectGLProcessor.cpp is the GPU backend. The test binary
    # compiles WITHOUT HAVE_OPENCV (the 8 base effects only) — the plugin .so
    # keeps the OpenCV-backed effects.
    ${DAQSTER_SOURCE_DIR}/src/plugins/demo_nodeditor_nodes_core/Sources/Video/VideoEffectOps.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/demo_nodeditor_nodes_core/Sources/Video/VideoTransformOps.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/demo_nodeditor_nodes_core/Sources/Video/VideoEffectGLProcessor.cpp
    # VideoEffectGLProcessor now uses the shared TexturePool (REQ-SW-PL-032
    # Issue #7) — compile the pool source so the test binary links standalone.
    ${DAQSTER_SOURCE_DIR}/src/plugins/common/GL/TexturePool.cpp
)

add_executable(demo_nodeditor_videooutput_tests ${VIDEO_OUTPUT_TEST_SOURCES})

target_include_directories(demo_nodeditor_videooutput_tests PRIVATE
    ${DAQSTER_SOURCE_DIR}/src/plugins/demo_nodeditor_nodes_core
    ${DAQSTER_SOURCE_DIR}/src/plugins/demo_nodeditor_nodes_core/Sources/Video
    ${DAQSTER_SOURCE_DIR}/src/plugins/common
    ${DAQSTER_SOURCE_DIR}/src/plugins/common/NodeDataTypes
    ${DAQSTER_SOURCE_DIR}/src/plugins/external_libs/nodeeditor/include
    ${DAQSTER_SOURCE_DIR}/src/frame_work/base/src/perf
    ${DEMO_CORE_EXPORT_INCLUDE_DIR}
)
target_compile_definitions(demo_nodeditor_videooutput_tests PRIVATE ${DEMO_CORE_EXPORT_DEFINITIONS})

target_link_libraries(demo_nodeditor_videooutput_tests PRIVATE
    Qt${QT_VERSION_MAJOR}::Test
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Multimedia
    QtNodes
    # VideoOutputNode.cpp now instruments via Daqster::Perf (REQ-SW-PL-027):
    # frame_work supplies the PerfProfiler symbols, the base/src/perf PUBLIC
    # include dir, and the DAQSTER_ENABLE_PERF compile definition.
    # Built-in nodes (NumberSource, NumberDisplay, Modulo, ArithmeticLogic) 
    # are in NodeEditorLibrary from node_editor_ide plugin.
    NodeEditorLibrary
    FrameworkCore
)

# QVideoWidget lives in Qt6::MultimediaWidgets (Qt5 bundles it in Multimedia).
# VideoGLBlitWidget (QOpenGLWidget shaders) needs the OpenGL module on both
# Qt versions.
if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    target_link_libraries(demo_nodeditor_videooutput_tests PRIVATE
        Qt${QT_VERSION_MAJOR}::MultimediaWidgets
        Qt${QT_VERSION_MAJOR}::OpenGLWidgets
        Qt${QT_VERSION_MAJOR}::OpenGL)
else()
    target_link_libraries(demo_nodeditor_videooutput_tests PRIVATE
        Qt${QT_VERSION_MAJOR}::OpenGL)
endif()

add_test(NAME demo_nodeditor_videooutput_tests
         COMMAND demo_nodeditor_videooutput_tests)
set_tests_properties(demo_nodeditor_videooutput_tests PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# ── VideoEffectNode CPU-path tests (REQ-SW-PL-039) ───────────────────────────
#
# QTEST_MAIN provides QApplication; the binary runs headless via the offscreen
# platform plugin. VideoEffectNode.cpp is compiled directly into the test
# binary (same pattern as demo_nodeditor_videooutput_tests) so the async CPU
# path on the shared ComputePool can be exercised without linking the plugin
# .so. The test binary compiles WITHOUT HAVE_OPENCV (the 8 base effects only).
set(VIDEO_EFFECT_TEST_SOURCES
    test_video_effect_node.cpp
    test_video_effect_node.h
    ${DAQSTER_SOURCE_DIR}/src/plugins/demo_nodeditor_nodes_core/Sources/Video/VideoEffectNode.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/demo_nodeditor_nodes_core/Sources/Video/VideoEffectOps.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/demo_nodeditor_nodes_core/Sources/Video/VideoTransformOps.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/demo_nodeditor_nodes_core/Sources/Video/VideoEffectGLProcessor.cpp
    # VideoEffectGLProcessor uses the shared TexturePool (REQ-SW-PL-032
    # Issue #7) and the CPU path uses the shared ComputePool (REQ-SW-PL-039) —
    # compile both pool sources so the test binary links standalone.
    ${DAQSTER_SOURCE_DIR}/src/plugins/common/GL/TexturePool.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/common/Threading/ComputePool.cpp
)

add_executable(demo_nodeditor_videoeffect_tests ${VIDEO_EFFECT_TEST_SOURCES})

target_include_directories(demo_nodeditor_videoeffect_tests PRIVATE
    ${DAQSTER_SOURCE_DIR}/src/plugins/demo_nodeditor_nodes_core
    ${DAQSTER_SOURCE_DIR}/src/plugins/demo_nodeditor_nodes_core/Sources/Video
    ${DAQSTER_SOURCE_DIR}/src/plugins/common
    ${DAQSTER_SOURCE_DIR}/src/plugins/common/NodeDataTypes
    ${DAQSTER_SOURCE_DIR}/src/plugins/external_libs/nodeeditor/include
    ${DEMO_CORE_EXPORT_INCLUDE_DIR}
)
target_compile_definitions(demo_nodeditor_videoeffect_tests PRIVATE ${DEMO_CORE_EXPORT_DEFINITIONS})

target_link_libraries(demo_nodeditor_videoeffect_tests PRIVATE
    Qt${QT_VERSION_MAJOR}::Test
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Multimedia
    QtNodes
    NodeEditorLibrary
    FrameworkCore
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    target_link_libraries(demo_nodeditor_videoeffect_tests PRIVATE
        Qt${QT_VERSION_MAJOR}::OpenGLWidgets
        Qt${QT_VERSION_MAJOR}::OpenGL)
else()
    target_link_libraries(demo_nodeditor_videoeffect_tests PRIVATE
        Qt${QT_VERSION_MAJOR}::OpenGL)
endif()

add_test(NAME demo_nodeditor_videoeffect_tests
         COMMAND demo_nodeditor_videoeffect_tests)
set_tests_properties(demo_nodeditor_videoeffect_tests PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# ── model-name serialization invariant (flow save/load round-trip) ──────────
#
# DataFlowGraphModel::loadNode resolves a node's type from
# <node>["internal-data"]["model-name"], a key written by the BASE
# NodeDelegateModel::save(). A model that overrode save() with a fresh
# QJsonObject dropped that key, so the node saved fine but resolved to an empty
# type on load — skipped as "<unnamed>" together with every connection touching
# it. Silent data loss with no save-time warning.
#
# The test instantiates each affected model and asserts save() carries
# "model-name". The engines (Pcap/GpuMonitor/JackDetect/AudioSource) are
# parented to the model and idle until start(), so constructing them is inert.
#
# Scope: the three *Obsolete* models (AudioSourceDataModelObsolete,
# DemuxNodeObsolete, MuxNodeObsolete) carry the same fix but are NOT covered
# here — instantiating them drags in the whole legacy QDevIO audio/display
# stack (QDevIoDisplayModelObsolete -> XYSeriesIODeviceObsolete ->
# QDevioDisplayModelUiObsolete, which needs QtCharts widgets), which would make
# this a GUI test for node types that are deprecated. The fix is the same
# one-liner in each; their coverage gap is noted rather than paid for.
set(MODEL_NAME_TEST_SOURCES
    test_model_name_serialization.cpp
    test_model_name_serialization.h
    ${DAQSTER_SOURCE_DIR}/src/plugins/demo_nodeditor_nodes_core/Displays/NumberDisplay/NumberDisplayDataModel.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/demo_nodeditor_nodes_core/Operators/ArithmeticLogic/ArithmeticLogicModel.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/demo_nodeditor_nodes_core/Operators/ModuloModel.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/demo_nodeditor_nodes_core/Sources/AudioSource/AudioSourceDataModel.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/demo_nodeditor_nodes_core/Sources/GpuMonitor/GpuMonitorEngine.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/demo_nodeditor_nodes_core/Sources/GpuMonitor/GpuMonitorModel.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/demo_nodeditor_nodes_core/Sources/JackDetect/JackDetectEngine.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/demo_nodeditor_nodes_core/Sources/JackDetect/JackDetectModel.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/demo_nodeditor_nodes_core/Sources/Pcap/PcapEngine.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/demo_nodeditor_nodes_core/Sources/Pcap/PcapModel.cpp
    # Transitive definitions the models reference but do not implement:
    # ExprParser backs ArithmeticLogicModel, MicCaptureWorker backs
    # AudioSourceDataModel.
    ${DAQSTER_SOURCE_DIR}/src/plugins/demo_nodeditor_nodes_core/Operators/ArithmeticLogic/ExprParser.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/demo_nodeditor_nodes_core/Sources/AudioSource/MicCaptureWorker.cpp
)

add_executable(demo_nodeditor_modelname_tests ${MODEL_NAME_TEST_SOURCES})

target_include_directories(demo_nodeditor_modelname_tests PRIVATE
    ${DAQSTER_SOURCE_DIR}/src/plugins/demo_nodeditor_nodes_core
    ${DAQSTER_SOURCE_DIR}/src/plugins/demo_nodeditor_nodes_core/Sources/AudioSource
    # MicCaptureWorker.h pulls AudioBufferToSampled.h from the Video sources.
    ${DAQSTER_SOURCE_DIR}/src/plugins/demo_nodeditor_nodes_core/Sources/Video
    ${DAQSTER_SOURCE_DIR}/src/plugins/common
    ${DAQSTER_SOURCE_DIR}/src/plugins/common/NodeDataTypes
    ${DAQSTER_SOURCE_DIR}/src/plugins/external_libs/nodeeditor/include
    ${DAQSTER_SOURCE_DIR}/src/frame_work/base/src/include
    ${DEMO_CORE_EXPORT_INCLUDE_DIR}
)
target_compile_definitions(demo_nodeditor_modelname_tests PRIVATE ${DEMO_CORE_EXPORT_DEFINITIONS})

target_link_libraries(demo_nodeditor_modelname_tests PRIVATE
    Qt${QT_VERSION_MAJOR}::Test
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Multimedia
    QtNodes
    # AudioCompat (used by AudioSourceDataModel's constructor) is a FrameworkCore
    # utility; the models themselves are compiled in above rather than linked
    # from the core plugin .so.
    FrameworkCore
)

add_test(NAME demo_nodeditor_modelname_tests
         COMMAND demo_nodeditor_modelname_tests)
set_tests_properties(demo_nodeditor_modelname_tests PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# ── ComputePool unit tests (REQ-SW-PL-039) ───────────────────────────────────
#
# QtCore-only (QTEST_GUILESS_MAIN, no widgets). The pool source is compiled
# directly into the test binary.
set(COMPUTE_POOL_TEST_SOURCES
    test_compute_pool.cpp
    test_compute_pool.h
    ${DAQSTER_SOURCE_DIR}/src/plugins/common/Threading/ComputePool.cpp
)

add_executable(demo_nodeditor_computepool_tests ${COMPUTE_POOL_TEST_SOURCES})

target_include_directories(demo_nodeditor_computepool_tests PRIVATE
    ${DAQSTER_SOURCE_DIR}/src/plugins/common
)

target_link_libraries(demo_nodeditor_computepool_tests PRIVATE
    Qt${QT_VERSION_MAJOR}::Test
    Qt${QT_VERSION_MAJOR}::Core
)

add_test(NAME demo_nodeditor_computepool_tests
         COMMAND demo_nodeditor_computepool_tests)
