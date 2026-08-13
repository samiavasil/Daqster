# Unit tests for the frame_work Perf module (REQ-SW-FW-008).
#
# The code under test lives in frame_work (Daqster::Perf). The test binary links
# frame_work directly and includes PerfProfiler.h via frame_work's PUBLIC
# include dir (base/src/perf). The tests run headless (QCoreApplication, no
# widgets) — QTEST_GUILESS_MAIN model with a shared main() in test_main.cpp.
#
# Three test classes (RollingStatsTest / DomainTest / StopwatchScopeTest) are
# declared in headers and share a single binary (perf_profiler_tests), mirroring
# the tests/plugins/demo_nodeditor_nodes pattern.

find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Test Core REQUIRED)

set(CMAKE_AUTOMOC ON)

set(TEST_SOURCES
    test_main.cpp
    test_rolling_stats.cpp test_rolling_stats.h
    test_domain.cpp test_domain.h
    test_stopwatch_scope.cpp test_stopwatch_scope.h
    perf_log_capture.h
)

add_executable(perf_profiler_tests ${TEST_SOURCES})

target_link_libraries(perf_profiler_tests PRIVATE
    frame_work
    Qt${QT_VERSION_MAJOR}::Test
    Qt${QT_VERSION_MAJOR}::Core
)

add_test(NAME perf_profiler_tests COMMAND $<TARGET_FILE:perf_profiler_tests>)
