# Unit tests for the nodeditor_nodes video plugin logic.
#
# The code under test (VideoTransformOps + StreamUrlValidator) is QtCore/QtGui
# only - no Multimedia, no QtNodes, no frame_work - so it is compiled directly
# into the test binary instead of linking the plugin .so. The tests run
# headless (QCoreApplication, no widgets).
#
# The test classes (TestVideoTransformOps/TestStreamUrlValidator/
# SampledDataTest/SampledStreamDescriptorTest/FftUtilTest/
# AudioBufferToSampledTest/TestVideoPerfBadge) are declared in headers
# and share a single binary (nodeditor_nodes_tests) whose main() lives in
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
# (nodeditor_nodes_core_export.h). These test binaries compile core sources
# directly instead of linking the plugin .so, so they need two things:
#   - the generated directory on their own include path (they do not inherit the
#     plugin's PUBLIC include directories);
#   - NODEDITOR_NODES_CORE_STATIC_DEFINE, which turns the export macro into
#     nothing. Without it the header would ask for __declspec(dllimport) on
#     Windows while the definitions are compiled straight into the test binary
#     (LNK2001 / C2491).
# Derive the generated-header directory from the core target itself. Using
# ${CMAKE_BINARY_DIR}/src/... only works when daqster is the top-level project;
# in a side-by-side build (private plugin repo add_subdirectory(../daqster))
# the subtree lives under ${CMAKE_BINARY_DIR}/daqster/, so the hardcoded path
# would miss nodeditor_nodes_core_export.h.
get_target_property(_DEMO_CORE_BINARY_DIR NodeditorNodesCore BINARY_DIR)
set(DEMO_CORE_EXPORT_INCLUDE_DIR ${_DEMO_CORE_BINARY_DIR})
set(DEMO_CORE_EXPORT_DEFINITIONS NODEDITOR_NODES_CORE_STATIC_DEFINE)

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
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/core/Sources/Video/VideoTransformOps.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/core/Sources/Video/StreamUrlValidator.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/core/Sources/Video/VideoPerfBadge.cpp
)

add_executable(nodeditor_nodes_tests ${TEST_SOURCES})

target_include_directories(nodeditor_nodes_tests PRIVATE
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/core/Sources/Video
    ${DAQSTER_SOURCE_DIR}/src/plugins/common
    ${DAQSTER_SOURCE_DIR}/src/plugins/external_libs/nodeeditor/include
    ${DEMO_CORE_EXPORT_INCLUDE_DIR}
)
target_compile_definitions(nodeditor_nodes_tests PRIVATE ${DEMO_CORE_EXPORT_DEFINITIONS})

target_link_libraries(nodeditor_nodes_tests PRIVATE
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

add_test(NAME nodeditor_nodes_tests COMMAND $<TARGET_FILE:nodeditor_nodes_tests>)

# ── DaqDisplayNode save/restore round-trip GUI tests ─────────────────────────
#
# DaqDisplayNode requires QApplication (QtCharts widgets) and the offscreen
# platform plugin. The node source is compiled directly into the test binary
# (same pattern as requirements_manager_gui_tests). QTEST_MAIN in
# test_daq_display_node.cpp provides the QApplication main.
#
# The QtChartsCompat.h header is in node_editor_ide/NodeSupport/display/.
# DaqDisplayNode.cpp transitively depends on FftUtil.h + SampledData.h (both
# header-only, in src/plugins/common/) and on SampledStreamDescriptor.h (in
# NodeDataTypes/).
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Charts REQUIRED)

set(DISPLAY_TEST_SOURCES
    test_daq_display_node.cpp
    test_daq_display_node.h
    # plugin logic under test (compiled directly into the test binary):
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/core/Displays/DaqDisplay/DaqDisplayNode.cpp
    # GenericDisplayNode is a thin DaqDisplayNode subclass (name/caption only) —
    # compiled so the alias behavior + registry resolution tests link standalone.
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/core/Displays/GenericDisplay/GenericDisplayNode.cpp
    # DaqDisplayWidget comes from the GUI plugin. It is compiled in rather than
    # linked from the .so so the rebuildCards() card-list invariant can be tested
    # directly — the freeze it caused was only reachable through a live audio
    # stream, which no headless test could reproduce.
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/gui/Displays/DaqDisplay/DaqDisplayWidget.cpp
    # DaqDisplayNode now submits compute to the shared ComputePool
    # (REQ-SW-PL-039) — compile the pool source so the test binary links
    # standalone.
    ${DAQSTER_SOURCE_DIR}/src/plugins/common/Threading/ComputePool.cpp
)

add_executable(demo_nodeditor_display_tests ${DISPLAY_TEST_SOURCES})

target_include_directories(demo_nodeditor_display_tests PRIVATE
    ${DAQSTER_SOURCE_DIR}/src/plugins/common
    ${DAQSTER_SOURCE_DIR}/src/plugins/common/NodeDataTypes
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/core
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/core/Displays/DaqDisplay
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/core/Displays/GenericDisplay
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/node_editor_ide/NodeSupport/display
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/gui
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/gui/Displays/DaqDisplay
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
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    # widget_rebuildCards_terminatesAndIsStable drives rebuildCards() directly.
    # The defect it guards against is an unbounded loop, so a regression shows
    # up as a hang rather than an assertion failure — bound it so CI fails fast
    # instead of sitting on the default 1500 s timeout.
    TIMEOUT 120)

# ── VideoFrameData value-semantic tests (REQ-SW-PL-020, AC 6) ────────────────
#
# QTEST_GUILESS_MAIN provides QCoreApplication — QImage operations do not
# need a widget-backed event loop. On Qt5 only the stub path is compiled;
# Qt6 brings QVideoFrame-based round-trip tests.
set(VIDEO_FRAME_TEST_SOURCES
    test_video_frame_data.cpp
    test_video_frame_data.h
    # VideoFrameData::releaseTextures() returns pooled textures to the global
    # TexturePool (REQ-SW-PL-038). The pool is NOT compiled here — it is linked
    # from DaqsterCommonLibrary below, so the test binary and the plugin share
    # ONE pool instead of each owning a private copy.
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
    DaqsterCommonLibrary
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
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/core/Sinks/Video/VideoOutputNode.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/core/Sources/Video/VideoPerfBadge.cpp
    # REQ-SW-PL-055: the model constructs no widget — it receives an
    # IVideoOutputUi from IWidgetProvider. The test is not a plugin host, so it
    # plays the GUI plugin's role and compiles the whole widget side directly:
    # VideoOutputUi plus BOTH display backends, because which one it builds
    # depends on detectVideoBackend() at runtime, not on the test's wishes.
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/gui/Sinks/Video/VideoOutputUi.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/gui/Sinks/Video/VideoGLBlitWidget.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/gui/Sinks/Video/VideoSoftwareWidget.cpp
    # Embedded effects (REQ-SW-PL-034): VideoOutputNode now uses
    # VideoEffectOps::allSpecs() + VideoEffectGLProcessor::processTexture().
    # VideoEffectOps.cpp delegates to VideoTransformOps.cpp for the CPU apply
    # functions; VideoEffectGLProcessor.cpp is the GPU backend. The test binary
    # compiles WITHOUT HAVE_OPENCV (the 8 base effects only) — the plugin .so
    # keeps the OpenCV-backed effects.
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/core/Sources/Video/VideoEffectOps.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/core/Sources/Video/VideoTransformOps.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/core/Sources/Video/VideoEffectGLProcessor.cpp
    # VideoEffectGLProcessor uses the shared TexturePool (REQ-SW-PL-032
    # Issue #7) — linked from DaqsterCommonLibrary below, one definition in the
    # process.
)

add_executable(demo_nodeditor_videooutput_tests ${VIDEO_OUTPUT_TEST_SOURCES})

target_include_directories(demo_nodeditor_videooutput_tests PRIVATE
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/core
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/core/Sources/Video
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/core/Sinks/Video
    # REQ-SW-PL-055: the GUI plugin's own root, because VideoOutputUi.h is
    # included as "VideoOutputUi.h" from the test.
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/gui
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/gui/Sources/Video
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/gui/Processing/Video
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/gui/Sinks/Video
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
    DaqsterCommonLibrary
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

# ── Audio resampler continuity tests (REQ-SW-PL-076, AC9) ────────────────────
#
# QtCore only (QTEST_GUILESS_MAIN): AudioResample.h depends on QVector and
# nothing else, so the continuity invariants run on CI without a sound card.
#
# The defect: the resampler used to be a pure per-block function (`srcPos =
# i / ratio` from 0 for every block), so a live microphone stream at 44.1 kHz
# played on a 48 kHz device was resampled with a phase restart 172 times per
# second — a click at every block boundary. Ratio 2.0 (TTS) hid it.
#
# The tests must FAIL on the stateless implementation: 300 blocks of 256
# samples do not match one block of 76 800 samples and the boundary jump is
# ~1.6x the largest jump inside a block.
add_executable(demo_nodeditor_audioresample_tests
    test_audio_resample.cpp
    test_audio_resample.h
)

target_include_directories(demo_nodeditor_audioresample_tests PRIVATE
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/core/shared
)

target_link_libraries(demo_nodeditor_audioresample_tests PRIVATE
    Qt${QT_VERSION_MAJOR}::Test
    Qt${QT_VERSION_MAJOR}::Core
)

add_test(NAME demo_nodeditor_audioresample_tests
         COMMAND demo_nodeditor_audioresample_tests)

# ── File Playback pacing tests (REQ-SW-PL-076) ──────────────────────────────
#
# QTEST_GUILESS_MAIN (QCoreApplication): the source node is widget-free and only
# reads a file — no audio device is opened, so the suite runs on CI without a
# sound card.
#
# The defect: startPlayback() divided the chunk size in BYTES by the sample rate
# in FRAMES/s, so a FLOAT32 stream (4 B/sample) got an interval 4x longer than
# its audio (92 ms for 23 ms) — the file played 4x too slowly and the sink
# underran on every block. `modelTimerUsesRealTimeInterval` asserts the QTimer
# interval the model really starts with, so it fails on that formula.
#
# FilePlaybackModel.cpp is compiled directly into the binary (same pattern as
# demo_nodeditor_audiooutput_tests); FrameworkCore is linked because
# `lcDemoNodes` lives in base/src/logging/LogCategories.cpp.
set(FILE_PLAYBACK_TEST_SOURCES
    test_file_playback_timing.cpp
    test_file_playback_timing.h
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/core/Sources/FilePlayback/FilePlaybackModel.cpp
)

add_executable(demo_nodeditor_fileplayback_tests ${FILE_PLAYBACK_TEST_SOURCES})

target_include_directories(demo_nodeditor_fileplayback_tests PRIVATE
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/core
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/core/shared
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/core/Sources/FilePlayback
    ${DAQSTER_SOURCE_DIR}/src/plugins/common
    ${DAQSTER_SOURCE_DIR}/src/plugins/common/NodeDataTypes
    ${DAQSTER_SOURCE_DIR}/src/plugins/external_libs/nodeeditor/include
    ${DAQSTER_SOURCE_DIR}/src/frame_work/base/src/include
    ${DAQSTER_SOURCE_DIR}/src/frame_work/base/src/logging
    ${DEMO_CORE_EXPORT_INCLUDE_DIR}
)
target_compile_definitions(demo_nodeditor_fileplayback_tests PRIVATE ${DEMO_CORE_EXPORT_DEFINITIONS})

target_link_libraries(demo_nodeditor_fileplayback_tests PRIVATE
    Qt${QT_VERSION_MAJOR}::Test
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    QtNodes
    FrameworkCore
)

add_test(NAME demo_nodeditor_fileplayback_tests
         COMMAND demo_nodeditor_fileplayback_tests)

# ── Capture buffer sizing (REQ-SW-PL-076) ────────────────────────────────────
#
# Header-only arithmetic (AudioCaptureBuffer::bytesFor) — no audio device and no
# QAudioSource is touched, so the suite runs on CI without a sound card.
#
# The defect it guards: MicCaptureWorker asked for a fixed 1000-byte capture
# buffer. On Qt5 that is a period (1000 B every ~5 ms = real time); on Qt6 it is
# the number of bytes the backend reads PER SERVER PERIOD, so the same request
# delivered one 1000 B block every 21.3 ms — 24 % of real time at 48 kHz mono
# f32 (measured). Everything downstream starved: AudioOutput got 5 ms of audio
# per 21 ms, its 200 ms device buffer never filled and the sound stuttered.
add_executable(demo_nodeditor_audiocapturebuffer_tests
    test_audio_capture_buffer.cpp
    test_audio_capture_buffer.h
)

target_include_directories(demo_nodeditor_audiocapturebuffer_tests PRIVATE
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/core
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/core/Sources/AudioSource
)

target_link_libraries(demo_nodeditor_audiocapturebuffer_tests PRIVATE
    Qt${QT_VERSION_MAJOR}::Test
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Multimedia
    # AudioCompat.h (the Qt5/Qt6 sample-type helpers) arrives through
    # FrameworkCore's PUBLIC include directories, not a local path.
    FrameworkCore
)

add_test(NAME demo_nodeditor_audiocapturebuffer_tests
         COMMAND demo_nodeditor_audiocapturebuffer_tests)

# ── AudioOutputNode behavioural tests (REQ-SW-PL-076) ────────────────────────
# QTEST_GUILESS_MAIN (QCoreApplication): the model is widget-free
# (embeddedWidget() is nullptr, REQ-SW-PL-051) and QAudioSink/QAudioOutput need
# no QApplication — only a Qt event loop, which the test provides.
#
# The test must pass on a box with NO sound card: the no-device path is driven
# with a deviceId that cannot resolve, and the device path is asserted only
# when AudioCompat actually returns a device. Both halves matter — a no-op that
# crashes and a sink created per block both "work" on screen.
#
# AudioOutputNode.cpp + AudioPlaybackEngine.cpp are compiled directly into the
# binary (same pattern as demo_nodeditor_videooutput_tests) instead of linking
# the plugin .so. FrameworkCore is linked because AudioCompat is a framework
# utility and lcDemoNodes (used by the engine) is defined in
# base/src/logging/LogCategories.cpp.
set(AUDIO_OUTPUT_TEST_SOURCES
    test_audio_output_node.cpp
    test_audio_output_node.h
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/core/Sinks/Audio/AudioOutputNode.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/core/Sinks/Audio/AudioPlaybackEngine.cpp
)

add_executable(demo_nodeditor_audiooutput_tests ${AUDIO_OUTPUT_TEST_SOURCES})

target_include_directories(demo_nodeditor_audiooutput_tests PRIVATE
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/core
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/core/Sinks/Audio
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/core/shared
    ${DAQSTER_SOURCE_DIR}/src/plugins/common
    ${DAQSTER_SOURCE_DIR}/src/plugins/common/NodeDataTypes
    ${DAQSTER_SOURCE_DIR}/src/plugins/external_libs/nodeeditor/include
    ${DAQSTER_SOURCE_DIR}/src/frame_work/base/src/include
    ${DEMO_CORE_EXPORT_INCLUDE_DIR}
)
target_compile_definitions(demo_nodeditor_audiooutput_tests PRIVATE ${DEMO_CORE_EXPORT_DEFINITIONS})

target_link_libraries(demo_nodeditor_audiooutput_tests PRIVATE
    Qt${QT_VERSION_MAJOR}::Test
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    # QAudioFormat / QAudioSink / QAudioDevice (AudioCompat).
    Qt${QT_VERSION_MAJOR}::Multimedia
    QtNodes
    FrameworkCore
)

add_test(NAME demo_nodeditor_audiooutput_tests
         COMMAND demo_nodeditor_audiooutput_tests)

# ── AudioOutputWidget GUI tests (REQ-SW-PL-076, AC6) ─────────────────────────
#
# QTEST_MAIN (QApplication) + QT_QPA_PLATFORM=offscreen: the widget is real
# QtWidgets code. The model and the engine are compiled in as in the headless
# suite above, plus the GUI plugin's widget — the test binary plays the role of
# NodeWidgetFactory, which cannot be linked without the whole plugin host.
#
# The node is configured with an id that does not resolve, so no device is
# opened during the GUI tests and the no-device path stays exercised on CI.
set(AUDIO_WIDGET_TEST_SOURCES
    test_audio_output_widget.cpp
    test_audio_output_widget.h
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/core/Sinks/Audio/AudioOutputNode.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/core/Sinks/Audio/AudioPlaybackEngine.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/gui/Sinks/Audio/AudioOutputWidget.cpp
)

add_executable(demo_nodeditor_audiowidget_tests ${AUDIO_WIDGET_TEST_SOURCES})

target_include_directories(demo_nodeditor_audiowidget_tests PRIVATE
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/core
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/core/Sinks/Audio
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/core/shared
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/gui
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/gui/Sinks/Audio
    ${DAQSTER_SOURCE_DIR}/src/plugins/common
    ${DAQSTER_SOURCE_DIR}/src/plugins/common/NodeDataTypes
    ${DAQSTER_SOURCE_DIR}/src/plugins/external_libs/nodeeditor/include
    ${DAQSTER_SOURCE_DIR}/src/frame_work/base/src/include
    ${DEMO_CORE_EXPORT_INCLUDE_DIR}
)
target_compile_definitions(demo_nodeditor_audiowidget_tests PRIVATE ${DEMO_CORE_EXPORT_DEFINITIONS})

target_link_libraries(demo_nodeditor_audiowidget_tests PRIVATE
    Qt${QT_VERSION_MAJOR}::Test
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Multimedia
    QtNodes
    FrameworkCore
)

add_test(NAME demo_nodeditor_audiowidget_tests
         COMMAND demo_nodeditor_audiowidget_tests)
set_tests_properties(demo_nodeditor_audiowidget_tests PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# ── AudioSinkNode behavioural tests (REQ-SW-PL-077) ─────────────────────────
#
# QTEST_GUILESS_MAIN (QCoreApplication): the recorder is widget-free
# (embeddedWidget() is nullptr) and writes a file — no audio device is opened,
# so the suite runs on CI without a sound card.
#
# The point of the suite is the defect that moved the node here: the private
# implementation truncated the file on every block, so a multi-block stream
# left only the last block on disk. `threeBlocksAreAllInOneValidWav` fails if
# that behaviour ever comes back, and the same test checks the RIFF/data sizes
# that a reader relies on.
#
# AudioSinkNode.cpp + AudioPlaybackEngine.cpp are compiled directly into the
# binary (same pattern as demo_nodeditor_audiooutput_tests); FrameworkCore is
# linked because `lcDemoNodes` lives in base/src/logging and AudioCompat is a
# framework utility.
set(AUDIO_SINK_TEST_SOURCES
    test_audio_sink_node.cpp
    test_audio_sink_node.h
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/core/Sinks/Audio/AudioSinkNode.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/core/Sinks/Audio/AudioPlaybackEngine.cpp
)

add_executable(demo_nodeditor_audiosink_tests ${AUDIO_SINK_TEST_SOURCES})

target_include_directories(demo_nodeditor_audiosink_tests PRIVATE
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/core
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/core/Sinks/Audio
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/core/shared
    ${DAQSTER_SOURCE_DIR}/src/plugins/common
    ${DAQSTER_SOURCE_DIR}/src/plugins/common/NodeDataTypes
    ${DAQSTER_SOURCE_DIR}/src/plugins/external_libs/nodeeditor/include
    ${DAQSTER_SOURCE_DIR}/src/frame_work/base/src/include
    ${DEMO_CORE_EXPORT_INCLUDE_DIR}
)
target_compile_definitions(demo_nodeditor_audiosink_tests PRIVATE ${DEMO_CORE_EXPORT_DEFINITIONS})

target_link_libraries(demo_nodeditor_audiosink_tests PRIVATE
    Qt${QT_VERSION_MAJOR}::Test
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Multimedia
    QtNodes
    FrameworkCore
)

add_test(NAME demo_nodeditor_audiosink_tests
         COMMAND demo_nodeditor_audiosink_tests)

# ── AudioSinkWidget GUI tests (REQ-SW-PL-077, AC6) ──────────────────────────
#
# QTEST_MAIN (QApplication) + QT_QPA_PLATFORM=offscreen: the widget is real
# QtWidgets code. The model is compiled in as in the headless suite above, plus
# the GUI plugin's widget — the test binary plays the role of NodeWidgetFactory,
# which cannot be linked without the whole plugin host.
#
# The node is never given a real output path and playback stays disabled, so no
# file and no device are opened during the GUI tests.
set(AUDIO_SINK_WIDGET_TEST_SOURCES
    test_audio_sink_widget.cpp
    test_audio_sink_widget.h
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/core/Sinks/Audio/AudioSinkNode.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/core/Sinks/Audio/AudioPlaybackEngine.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/gui/Sinks/Audio/AudioSinkWidget.cpp
)

add_executable(demo_nodeditor_audiosinkwidget_tests ${AUDIO_SINK_WIDGET_TEST_SOURCES})

target_include_directories(demo_nodeditor_audiosinkwidget_tests PRIVATE
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/core
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/core/Sinks/Audio
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/core/shared
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/gui
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/gui/Sinks/Audio
    ${DAQSTER_SOURCE_DIR}/src/plugins/common
    ${DAQSTER_SOURCE_DIR}/src/plugins/common/NodeDataTypes
    ${DAQSTER_SOURCE_DIR}/src/plugins/external_libs/nodeeditor/include
    ${DAQSTER_SOURCE_DIR}/src/frame_work/base/src/include
    ${DEMO_CORE_EXPORT_INCLUDE_DIR}
)
target_compile_definitions(demo_nodeditor_audiosinkwidget_tests PRIVATE ${DEMO_CORE_EXPORT_DEFINITIONS})

target_link_libraries(demo_nodeditor_audiosinkwidget_tests PRIVATE
    Qt${QT_VERSION_MAJOR}::Test
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Multimedia
    QtNodes
    FrameworkCore
)

add_test(NAME demo_nodeditor_audiosinkwidget_tests
         COMMAND demo_nodeditor_audiosinkwidget_tests)
set_tests_properties(demo_nodeditor_audiosinkwidget_tests PROPERTIES
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
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/core/Processing/Video/VideoEffectNode.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/core/Sources/Video/VideoEffectOps.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/core/Sources/Video/VideoTransformOps.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/core/Sources/Video/VideoEffectGLProcessor.cpp
    # VideoEffectGLProcessor uses the shared TexturePool (REQ-SW-PL-032
    # Issue #7) — linked from DaqsterCommonLibrary below; the CPU path uses the
    # shared ComputePool (REQ-SW-PL-039), which is still compiled here because it
    # has no library target yet.
    ${DAQSTER_SOURCE_DIR}/src/plugins/common/Threading/ComputePool.cpp
)

add_executable(demo_nodeditor_videoeffect_tests ${VIDEO_EFFECT_TEST_SOURCES})

target_include_directories(demo_nodeditor_videoeffect_tests PRIVATE
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/core
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/core/Sources/Video
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/core/Processing/Video
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
    DaqsterCommonLibrary
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
# Scope: AudioSourceDataModelObsolete carries the same base-delegated fix but
# is still NOT covered here — its constructor chain pulls the full legacy audio
# worker stack (AudioNodeQdevIoConnectorObsolete → QDevIoDisplayModelObsolete +
# XYSeriesIODeviceObsolete + EventThreadPullObsolete + AudioWorkerObsolete),
# which is widget-free after the REQ-SW-PL-056 split but heavy; the node is
# however constructed headless by the lifecycle tests and its save() fix is
# the same one-liner.
# REQ-SW-PL-056 split (2026-10-08): DemuxNodeObsolete + MuxNodeObsolete ARE
# covered now — previously excluded because their chain dragged in
# QDevioDisplayModelUiObsolete (QtCharts widgets), which would have made this
# a GUI test for deprecated node types. The split moved the VIEW to the GUI
# plugin, so the pair compiles and runs in this widget-free binary.
set(MODEL_NAME_TEST_SOURCES
    test_model_name_serialization.cpp
    test_model_name_serialization.h
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/core/Displays/NumberDisplay/NumberDisplayDataModel.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/core/Operators/ArithmeticLogic/ArithmeticLogicModel.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/core/Operators/ModuloModel.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/core/Sources/AudioSource/AudioSourceDataModel.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/core/Sources/GpuMonitor/GpuMonitorEngine.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/core/Sources/GpuMonitor/GpuMonitorModel.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/core/Sources/JackDetect/JackDetectEngine.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/core/Sources/JackDetect/JackDetectModel.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/core/Sources/Pcap/PcapEngine.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/core/Sources/Pcap/PcapModel.cpp
    # Transitive definitions the models reference but do not implement:
    # ExprParser backs ArithmeticLogicModel, MicCaptureWorker backs
    # AudioSourceDataModel.
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/core/Operators/ArithmeticLogic/ExprParser.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/core/Sources/AudioSource/MicCaptureWorker.cpp
    # REQ-SW-PL-056: Demux/Mux + their GenericQDevIo connector dependency
    # (header-only StreamChannelObsolete/QDevIOStreamConfigObsolete types);
    # GenericQDevIoConnectorObsolete's base ctor comes from
    # NodeDataModelToQIODeviceConnectorObsolete.
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/core/Routing/Demux/DemuxNodeObsolete.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/core/Routing/Mux/MuxNodeObsolete.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/node_editor_ide/NodeSupport/connectors/GenericQDevIoConnectorObsolete.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/node_editor_ide/NodeSupport/connectors/NodeDataModelToQIODeviceConnectorObsolete.cpp
)

add_executable(demo_nodeditor_modelname_tests ${MODEL_NAME_TEST_SOURCES})

target_include_directories(demo_nodeditor_modelname_tests PRIVATE
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/core
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/core/Sources/AudioSource
    # MicCaptureWorker.h pulls AudioBufferToSampled.h from the Video sources.
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/core/Sources/Video
    # Demux/Mux headers include GenericQDevIoConnectorObsolete.h (connectors)
    # and StreamChannelObsolete.h / QDevIOStreamConfigObsolete.h (types).
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/node_editor_ide/NodeSupport/connectors
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/node_editor_ide/NodeSupport/types
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

# ── VideoFrameData / TexturePool consolidation invariants (REQ-SW-PL-032, AC 12) ─
#
# QTEST_MAIN (QGuiApplication, not QCoreApplication): the GL checks build a
# QOpenGLContext / QOffscreenSurface through VideoGLContextManager, and Qt
# segfaults doing that without a QGuiApplication. Qt::Widgets is deliberately
# NOT linked, so QTest builds a plain QGuiApplication.
#
# Links DaqsterCommonLibrary rather than compiling TexturePool.cpp, so the pool
# under test is the same singleton the plugins use (REQ-SW-PL-032 E1, commit
# f6f184d) — a test with its own copy would pass while the plugins leaked a
# texture per frame.
set(CONSOLIDATION_TEST_SOURCES
    test_video_frame_consolidation.cpp
    test_video_frame_consolidation.h
)

add_executable(demo_nodeditor_consolidation_tests ${CONSOLIDATION_TEST_SOURCES})

target_include_directories(demo_nodeditor_consolidation_tests PRIVATE
    ${DAQSTER_SOURCE_DIR}/src/plugins/common
    ${DAQSTER_SOURCE_DIR}/src/plugins/common/NodeDataTypes
    ${DAQSTER_SOURCE_DIR}/src/plugins/external_libs/nodeeditor/include
)

target_link_libraries(demo_nodeditor_consolidation_tests PRIVATE
    Qt${QT_VERSION_MAJOR}::Test
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Multimedia
    QtNodes
    NodeEditorLibrary
    DaqsterCommonLibrary
    # VideoGLContextManager is header-only but QOpenGLContext /
    # QOffscreenSurface live in QtGui + (Qt6) QtOpenGLWidgets.
)
if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    target_link_libraries(demo_nodeditor_consolidation_tests PRIVATE
        Qt${QT_VERSION_MAJOR}::OpenGL
        Qt${QT_VERSION_MAJOR}::OpenGLWidgets)
else()
    target_link_libraries(demo_nodeditor_consolidation_tests PRIVATE
        Qt${QT_VERSION_MAJOR}::OpenGL)
endif()

add_test(NAME demo_nodeditor_consolidation_tests
         COMMAND demo_nodeditor_consolidation_tests)
set_tests_properties(demo_nodeditor_consolidation_tests PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# ── FrameSamplerNode behavioural tests (REQ-SW-PL-030, AC 6) ─────────────────
#
# QTEST_GUILESS_MAIN — the model owns no widget (embeddedWidget() is nullptr
# by REQ-SW-PL-051), so no QApplication, no OpenCV and no GL context. The
# node source is compiled directly into the test binary (same pattern as
# demo_nodeditor_videooutput_tests).
set(FRAME_SAMPLER_TEST_SOURCES
    test_frame_sampler_node.cpp
    test_frame_sampler_node.h
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/core/Processing/Video/FrameSamplerNode.cpp
)

add_executable(demo_nodeditor_framesampler_tests ${FRAME_SAMPLER_TEST_SOURCES})

target_include_directories(demo_nodeditor_framesampler_tests PRIVATE
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/core
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/core/Sources/Video
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/core/Processing/Video
    ${DAQSTER_SOURCE_DIR}/src/plugins/common
    ${DAQSTER_SOURCE_DIR}/src/plugins/common/NodeDataTypes
    ${DAQSTER_SOURCE_DIR}/src/plugins/external_libs/nodeeditor/include
    ${DEMO_CORE_EXPORT_INCLUDE_DIR}
)
target_compile_definitions(demo_nodeditor_framesampler_tests PRIVATE ${DEMO_CORE_EXPORT_DEFINITIONS})

target_link_libraries(demo_nodeditor_framesampler_tests PRIVATE
    Qt${QT_VERSION_MAJOR}::Test
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    # VideoFrameData::type()/hasFrame() and QVideoFrame(QImage) — Multimedia.
    Qt${QT_VERSION_MAJOR}::Multimedia
    QtNodes
    # VideoFrameData's destructor calls releaseTextures(), which reaches
    # TexturePool::release() on the pooled path (REQ-SW-PL-032 E1). The pool is
    # linked, not compiled in, so this test binary shares the one process-wide
    # pool the plugins use.
    DaqsterCommonLibrary
)

add_test(NAME demo_nodeditor_framesampler_tests
         COMMAND demo_nodeditor_framesampler_tests)

# ── IStoppable lifecycle tests (REQ-SW-PL-050, AC 7) ─────────────────────────
#
# QTEST_GUILESS_MAIN. Reuses the model-name suite's source list: both targets
# instantiate the same hardware-free engines (SystemMonitor, GpuMonitor,
# JackDetect, Pcap), so the engine sources are compiled in rather than linked
# from the core plugin .so.
set(LIFECYCLE_TEST_SOURCES
    test_lifecycle_main.cpp
    test_thread_lifecycle.cpp
    test_thread_lifecycle.h
    ${MODEL_NAME_TEST_SOURCES}
    # SystemMonitorModel is not in MODEL_NAME_TEST_SOURCES (the model-name
    # suite covers a different set) but it IS one of the four nodes whose
    # IStoppable contract this target asserts. Its engine carries the actual
    # poller thread, so it has to come along too — the lifecycle tests are
    # about that thread being joined.
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/core/Sources/SystemMonitor/SystemMonitorModel.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/core/Sources/SystemMonitor/SystemMonitorEngine.cpp

    # ── The rest of the IStoppable roster (REQ-SW-PL-050, AC 7) ───────────────
    #
    # The suite's whole premise is that HeadlessEngine::stopAllNodes() casts
    # every model in the graph and calls stop() on it. Covering only the four
    # models whose pollers can also be *started* left 14 of 18 IStoppable
    # models unchecked — and the assertions that matter here (is the cast
    # reachable, is stop() idempotent, is destruction without stop() safe) need
    # no device at all. Only start() needs the hardware, so only the two
    # active-work tests stay restricted to the device-free four.
    #
    # Every constructor below is inert without its device: pollers/timers/
    # threads are created but not started, and the two models that touch
    # QMediaDevices (AudioSource, CameraSource) only *enumerate* — both calls
    # return empty under QCoreApplication, which the modelname suite already
    # exercises for AudioSourceDataModel.
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/core/Displays/DaqDisplay/DaqDisplayNode.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/core/Sinks/FileRecord/FileRecordModel.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/core/Sinks/NetworkSink/NetworkSinkModel.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/core/Sources/FilePlayback/FilePlaybackModel.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/core/Sources/Gamepad/GamepadModel.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/core/Sources/Gamepad/GamepadEngine.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/core/Sources/LLamaSource/LLamaModelDataModel.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/core/Sources/NetworkSource/NetworkSourceModel.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/core/Sources/Video/CameraSourceNode.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/core/Sources/Video/StreamSourceNode.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/core/Sources/Video/VideoFileSourceNode.cpp

    # Transitive definitions those models reference but do not implement:
    # StreamUrlValidator backs StreamSourceNode; DaqDisplayNode submits to the
    # shared ComputePool (REQ-SW-PL-039); VideoFrameData::releaseTextures()
    # reaches TexturePool, which E1 moved into the common GL library.
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/core/Sources/Video/StreamUrlValidator.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/common/Threading/ComputePool.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/common/GL/TexturePool.cpp
)

# PlutoSdrEngine calls iio_create_context_from_uri() and friends, so the model
# only links where the SDK is present. The plugin builds it unconditionally
# (src/plugins/node_editor/nodeditor_nodes/core/CMakeLists.txt:152), which is a build
# -graph question of its own; here the model is simply skipped when libiio is
# absent, so the lifecycle assertions stay runnable on a box without the SDK.
if(IIO_FOUND)
    list(APPEND LIFECYCLE_TEST_SOURCES
        ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/core/Sources/PlutoSdr/PlutoSdrModel.cpp
        ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/core/Sources/PlutoSdr/PlutoSdrEngine.cpp
    )
endif()

add_executable(demo_nodeditor_lifecycle_tests ${LIFECYCLE_TEST_SOURCES})

target_include_directories(demo_nodeditor_lifecycle_tests PRIVATE
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/core
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/core/Sources/AudioSource
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/core/Sources/Video
    ${DAQSTER_SOURCE_DIR}/src/plugins/common
    ${DAQSTER_SOURCE_DIR}/src/plugins/common/NodeDataTypes
    ${DAQSTER_SOURCE_DIR}/src/plugins/external_libs/nodeeditor/include
    ${DAQSTER_SOURCE_DIR}/src/frame_work/base/src/include
    ${DEMO_CORE_EXPORT_INCLUDE_DIR}
)
target_compile_definitions(demo_nodeditor_lifecycle_tests PRIVATE
    ${DEMO_CORE_EXPORT_DEFINITIONS}
    # Suppresses QTEST_MAIN in test_model_name_serialization.cpp — this target
    # registers both test classes from test_lifecycle_main.cpp instead.
    DAQSTER_SHARED_TESTS_PROVIDE_MAIN)

# PlutoSdrEngine keys its iio_* calls off HAVE_LIBIIO exactly as the plugin
# does, and the suite's model count depends on whether the model is in the
# build at all, so both the flag and the SDK headers have to reach this target.
if(IIO_FOUND)
    target_compile_definitions(demo_nodeditor_lifecycle_tests PRIVATE HAVE_LIBIIO)
    target_include_directories(demo_nodeditor_lifecycle_tests PRIVATE ${IIO_INCLUDE_DIRS})
    target_link_libraries(demo_nodeditor_lifecycle_tests PRIVATE ${IIO_LIBRARIES})
endif()

target_link_libraries(demo_nodeditor_lifecycle_tests PRIVATE
    Qt${QT_VERSION_MAJOR}::Test
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Multimedia
    QtNodes
    FrameworkCore
)

add_test(NAME demo_nodeditor_lifecycle_tests
         COMMAND demo_nodeditor_lifecycle_tests)
# stopWithActiveWorkIsSafe joins the poller threads. If a regression breaks the
# join, the binary hangs rather than failing an assertion — bound it so CI
# fails fast instead of sitting on the default 1500 s timeout.
set_tests_properties(demo_nodeditor_lifecycle_tests PROPERTIES TIMEOUT 120)

# ── core/gui split guard (REQ-SW-PL-051, AC 3 / AC 7; hardened by
#    REQ-SW-PL-056) ─────────────────────────────────────────────────────────
#
# Static analysis rather than a C++ test: the invariant is a build-graph
# property ("_core names no QtWidgets class"), which no runtime assertion can
# observe. Runs as a `cmake -P` script so it needs no toolchain and works on
# Windows without a POSIX shell.
#
# The scan uses the _core TARGET's own SOURCES (get_target_property) instead of
# a directory GLOB: _core compiles translation units physically under
# node_editor_ide/NodeSupport, which the old tree scan blind-spotted. The
# scanned count is pinned via CORE_EXPECTED_SOURCES (same filtered property —
# guards against a silently-empty query).
get_target_property(_CORE_SOURCES NodeditorNodesCore SOURCES)
set(_CORE_EXPECTED_SOURCES 0)
foreach(_CORE_S ${_CORE_SOURCES})
    if(_CORE_S MATCHES "\\.(cpp|cc|cxx|h|hpp)$")
        math(EXPR _CORE_EXPECTED_SOURCES "${_CORE_EXPECTED_SOURCES}+1")
    endif()
endforeach()
add_test(NAME demo_nodeditor_core_widgets_free
         COMMAND ${CMAKE_COMMAND}
                 "-DCORE_DIR=${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/core"
                 "-DCORE_SOURCES=${_CORE_SOURCES}"
                 "-DCORE_EXPECTED_SOURCES=${_CORE_EXPECTED_SOURCES}"
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/check_core_widgets_free.cmake)

# ── nodeWidgetDialogParent() embedded-widget dialog parenting ────────────────
#
# A node widget embedded by QGraphicsProxyWidget is an offscreen top-level
# window; a modal dialog parented to it grabs input but is never shown, so the
# app looks frozen (the VideoFileSource "Add video" button did this). The helper
# must resolve a real parent. Pure QtWidgets logic, but needs QApplication and
# the offscreen platform plugin. Header-only code under test lives in the GUI
# plugin's root, so that directory is the include path.
add_executable(demo_nodeditor_nodewidget_tests
    test_node_widget_dialogs.cpp
    test_node_widget_dialogs.h
)

target_include_directories(demo_nodeditor_nodewidget_tests PRIVATE
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor/nodeditor_nodes/gui
)

target_link_libraries(demo_nodeditor_nodewidget_tests PRIVATE
    Qt${QT_VERSION_MAJOR}::Test
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Widgets
)

add_test(NAME demo_nodeditor_nodewidget_tests
         COMMAND demo_nodeditor_nodewidget_tests)
set_tests_properties(demo_nodeditor_nodewidget_tests PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
