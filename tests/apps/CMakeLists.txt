# App-level integration tests.
#
# node_runner_headless_registry (REQ-SW-PL-056 AC2/AC4) runs the real
# NodeRunner binary over two shipped fixtures and asserts on its output:
# the six components that used to be missing from registerNodesHeadless()
# must instantiate headless, and a genuinely unknown type must still be
# rejected with a message that names it.
#
# The check lives in check_headless_registry.cmake rather than in a shell
# script because ctest runs on both Linux and Windows, and because NodeRunner
# does not exit on its own — bounding the run needs process control that is
# portable. The fixtures are DATA: their path is passed in at test time.

add_test(NAME node_runner_headless_registry
         COMMAND ${CMAKE_COMMAND}
                 "-DNODE_RUNNER=$<TARGET_FILE:NodeRunner>"
                 "-DFLOW_DIR=${DAQSTER_SOURCE_DIR}/tests/data"
                 -P "${CMAKE_CURRENT_SOURCE_DIR}/check_headless_registry.cmake")

# Run inside the script: negative verdict, then the positive fixture. The
# headless platform is a test/CI convention, not a product hardcode: after
# REQ-SW-PL-056 NodeRunner itself no longer sets QT_QPA_PLATFORM (see main.cpp),
# so the offscreen plugin is declared here, on the test.
#
# Worst case inside the script: negative run up to 45 s (PulseAudio stall budget,
# normally 0.5 s) + positive run 10 s (killed by its own TIMEOUT, that IS its
# runtime) + one 45 s retry when the first window saw no "flow loaded
# successfully" — 100 s plus plugin-load overhead. 180 leaves the ctest timeout
# where it belongs: catching a genuinely wedged process, not a stalled device.
#
# RESOURCE_LOCK audio: the positive fixture is one node of every previously
# missing component, including AudioSourceObsolete/AudioDisplay, and the run is
# ended by execute_process(TIMEOUT) → SIGTERM. The check script records the
# consequence itself ("the abrupt termination can leave PulseAudio/PipeWire in a
# state that hangs a subsequent run"). Measured: in parallel with
# demo_nodeditor_lifecycle_tests this made the latter's 3000 ms QAudioSource
# thread join time out 2 of 3 runs. Same lock name as the two audio suites in
# tests/plugins/nodeditor_nodes/CMakeLists.txt. The lock serialises TESTS, not
# devices: a process from an earlier run can still hold PulseAudio, which is why
# the script additionally budgets for the ~25 s connect stall — measured
# 2026-11-14, missing_node.flow took 30.66 s and died on the old 10 s cap.
set_tests_properties(node_runner_headless_registry PROPERTIES
    TIMEOUT 180
    RESOURCE_LOCK audio
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
