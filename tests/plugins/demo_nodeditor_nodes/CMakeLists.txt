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

find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Test Multimedia REQUIRED)

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
    # plugin logic under test (compiled directly into the test binary):
    ${DAQSTER_SOURCE_DIR}/src/plugins/demo_nodeditor_nodes/Sources/Video/VideoTransformOps.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/demo_nodeditor_nodes/Sources/Video/StreamUrlValidator.cpp
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
