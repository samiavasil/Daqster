# Unit tests for the shared plugin infrastructure in src/plugins/common/
# (REQ-SW-PL-050): ThreadOwner + ProcessOwner, and the local server supervisor
# (REQ-SW-PL-081).
#
# Mirror of src/plugins/: the primitives are plugin-side code (header + one .cpp
# in DaqsterCommonLibrary), so they are tested here rather than under a node
# plugin. The sources are compiled DIRECTLY into the binary — the same pattern
# ComputePool.cpp uses in the node suites — so this target stays free of
# Gui/OpenGL from the shared library and of QApplication.
#
# Headless (QCoreApplication): every primitive is QtCore (the server supervisor
# also uses QtNetwork). QThread::finished → deleteLater, QProcess::finished →
# deleteLater and QNetworkReply::finished → deleteLater need the event
# dispatcher, which the shared main() in test_main.cpp provides.

set(CMAKE_AUTOMOC ON)

# QtTest is not pulled in by every parent target; ask for it here so this
# directory is self-contained (same as the node suites).
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Test Network REQUIRED)

set(COMMON_TEST_SOURCES
    test_main.cpp
    test_thread_owner.cpp test_thread_owner.h
    test_process_owner.cpp test_process_owner.h
    test_local_server_service.cpp test_local_server_service.h
    # REQ-SW-PL-087: RFC 6455 client against a tiny in-test server.
    test_websocket_client.cpp test_websocket_client.h
    # plugin logic under test (compiled directly into the test binary):
    ${DAQSTER_SOURCE_DIR}/src/plugins/common/Threading/ProcessOwner.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/common/Threading/ThreadingLog.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/common/Servers/LocalServerService.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/common/WebSocket/WebSocketClient.cpp
)

add_executable(daqster_common_tests ${COMMON_TEST_SOURCES})

target_include_directories(daqster_common_tests PRIVATE
    ${DAQSTER_SOURCE_DIR}/src/plugins/common
)

target_link_libraries(daqster_common_tests PRIVATE
    Qt${QT_VERSION_MAJOR}::Test
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Network
)

# ── REQ-SW-PL-081: stub server for TestLocalServerService ───────────────
# A separate executable, not a shell script: `LocalServerService` spawns a
# **process** and waits for readiness over `GET /health`, so the stub has to
# really listen on a port. QtCore + QtNetwork, zero model lines, zero GPU. It is
# built only for the test (never installed).
add_executable(local_server_stub local_server_stub.cpp)
target_link_libraries(local_server_stub PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Network
)
add_dependencies(daqster_common_tests local_server_stub)
target_compile_definitions(daqster_common_tests PRIVATE
    LOCAL_SERVER_STUB="$<TARGET_FILE:local_server_stub>"
)

add_test(NAME daqster_common_tests COMMAND daqster_common_tests)
# stopTimesOutAndTerminates and killFallbackOnStubbornProcess wait for real
# deadlines (up to ~4 s each), and the server tests start real stub processes
# (a few seconds each). If a regression removes a deadline the binary hangs
# instead of failing an assertion — bound it so CI fails fast.
set_tests_properties(daqster_common_tests PROPERTIES TIMEOUT 180)
