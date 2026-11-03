# Unit tests for the shared plugin infrastructure in src/plugins/common/
# (REQ-SW-PL-050): ThreadOwner + ProcessOwner.
#
# Mirror of src/plugins/: the primitives are plugin-side code (header + one .cpp
# in DaqsterCommonLibrary), so they are tested here rather than under a node
# plugin. The sources are compiled DIRECTLY into the binary — the same pattern
# ComputePool.cpp uses in the node suites — so this target stays QtCore-only
# (no Gui/OpenGL from the shared library, no QApplication).
#
# Headless (QCoreApplication): both primitives are QtCore. QThread::finished →
# deleteLater and QProcess::finished → deleteLater need the event dispatcher,
# which the shared main() in test_main.cpp provides.

set(CMAKE_AUTOMOC ON)

# QtTest is not pulled in by every parent target; ask for it here so this
# directory is self-contained (same as the node suites).
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Test REQUIRED)

set(COMMON_TEST_SOURCES
    test_main.cpp
    test_thread_owner.cpp test_thread_owner.h
    test_process_owner.cpp test_process_owner.h
    # plugin logic under test (compiled directly into the test binary):
    ${DAQSTER_SOURCE_DIR}/src/plugins/common/Threading/ProcessOwner.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/common/Threading/ThreadingLog.cpp
)

add_executable(daqster_common_tests ${COMMON_TEST_SOURCES})

target_include_directories(daqster_common_tests PRIVATE
    ${DAQSTER_SOURCE_DIR}/src/plugins/common
)

target_link_libraries(daqster_common_tests PRIVATE
    Qt${QT_VERSION_MAJOR}::Test
    Qt${QT_VERSION_MAJOR}::Core
)

add_test(NAME daqster_common_tests COMMAND daqster_common_tests)
# stopTimesOutAndTerminates and killFallbackOnStubbornProcess wait for real
# deadlines (up to ~4 s each). If a regression removes the deadline the binary
# hangs instead of failing an assertion — bound it so CI fails fast.
set_tests_properties(daqster_common_tests PROPERTIES TIMEOUT 120)
