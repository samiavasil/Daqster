# Unit tests for the demo_nodeditor_nodes video plugin logic.
#
# The code under test (VideoTransformOps + StreamUrlValidator) is QtCore/QtGui
# only - no Multimedia, no QtNodes, no frame_work - so it is compiled directly
# into the test binary instead of linking the plugin .so. The tests run
# headless (QCoreApplication, no widgets).
#
# The five test classes (TestVideoTransformOps/TestStreamUrlValidator/
# SampledDataTest/FftUtilTest/AudioBufferToSampledTest) are declared in headers
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

set(TEST_SOURCES
    test_main.cpp
    test_video_transform_ops.cpp test_video_transform_ops.h
    test_stream_url_validator.cpp test_stream_url_validator.h
    test_sampled_data.cpp test_sampled_data.h
    test_fft_util.cpp test_fft_util.h
    test_audio_buffer_to_sampled.cpp test_audio_buffer_to_sampled.h
    test_video_perf_badge.cpp test_video_perf_badge.h
    # plugin logic under test (compiled directly into the test binary):
    ${DAQSTER_SOURCE_DIR}/src/plugins/demo_nodeditor_nodes/Sources/Video/VideoTransformOps.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/demo_nodeditor_nodes/Sources/Video/StreamUrlValidator.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/demo_nodeditor_nodes/Sources/Video/VideoPerfBadge.cpp
)

add_executable(demo_nodeditor_nodes_tests ${TEST_SOURCES})

target_include_directories(demo_nodeditor_nodes_tests PRIVATE
    ${DAQSTER_SOURCE_DIR}/src/plugins/demo_nodeditor_nodes/Sources/Video
    ${DAQSTER_SOURCE_DIR}/src/plugins/common
    ${DAQSTER_SOURCE_DIR}/src/plugins/external_libs/nodeeditor/include
)

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
    ${DAQSTER_SOURCE_DIR}/src/plugins/demo_nodeditor_nodes/Displays/DaqDisplay/DaqDisplayNode.cpp
)

add_executable(demo_nodeditor_display_tests ${DISPLAY_TEST_SOURCES})

target_include_directories(demo_nodeditor_display_tests PRIVATE
    ${DAQSTER_SOURCE_DIR}/src/plugins/common
    ${DAQSTER_SOURCE_DIR}/src/plugins/common/NodeDataTypes
    ${DAQSTER_SOURCE_DIR}/src/plugins/demo_nodeditor_nodes/Displays/DaqDisplay
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor_ide/BuiltInNodes/Library/display
    ${DAQSTER_SOURCE_DIR}/src/plugins/external_libs/nodeeditor/include
)

target_link_libraries(demo_nodeditor_display_tests PRIVATE
    Qt${QT_VERSION_MAJOR}::Test
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Charts
    QtNodes
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
    ${DAQSTER_SOURCE_DIR}/src/plugins/demo_nodeditor_nodes/Sources/Video/VideoOutputNode.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/demo_nodeditor_nodes/Sources/Video/VideoPerfBadge.cpp
    # VideoOutputNode references VideoGLBlitWidget (DAQSTER_GL_BLIT display path);
    # compile the widget source so the test binary links standalone:
    ${DAQSTER_SOURCE_DIR}/src/plugins/demo_nodeditor_nodes/Sources/Video/VideoGLBlitWidget.cpp
)

add_executable(demo_nodeditor_videooutput_tests ${VIDEO_OUTPUT_TEST_SOURCES})

target_include_directories(demo_nodeditor_videooutput_tests PRIVATE
    ${DAQSTER_SOURCE_DIR}/src/plugins/demo_nodeditor_nodes/Sources/Video
    ${DAQSTER_SOURCE_DIR}/src/plugins/common
    ${DAQSTER_SOURCE_DIR}/src/plugins/common/NodeDataTypes
    ${DAQSTER_SOURCE_DIR}/src/plugins/external_libs/nodeeditor/include
)

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
    frame_work
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
