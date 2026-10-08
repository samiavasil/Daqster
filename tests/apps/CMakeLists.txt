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

# Two bounded runs of NodeRunner (15 s cap each) plus margin. The headless
# platform is a test/CI convention, not a product hardcode: after REQ-SW-PL-056
# NodeRunner itself no longer sets QT_QPA_PLATFORM (see main.cpp), so the
# offscreen plugin is declared here, on the test.
set_tests_properties(node_runner_headless_registry PROPERTIES
    TIMEOUT 120
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
