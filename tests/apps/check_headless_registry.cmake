# Headless registry acceptance check — REQ-SW-PL-056 AC2 + AC4.
#
# Runs the real NodeRunner binary over two shipped fixtures:
#
#   headless_all_components.flow
#       One node of every type that was missing from registerNodesHeadless()
#       before this work item (VideoOutput, DaqDisplay, GenericDisplay,
#       AudioDisplay, AudioDisplayObsolete, AudioSourceObsolete). All six must
#       CONSTRUCT and the flow must load — that is AC4 end to end, including
#       the two types no other fixture names.
#
#   missing_node.flow
#       Names "BogusUnregisteredNode", which no registry can resolve. The run
#       must FAIL with exit code 1 and print the fail-fast message — that is
#       AC2, which stays meaningful for genuinely unknown types after the six
#       were registered.
#
# NodeRunner does not terminate on its own: it runs the event loop until it is
# stopped. The positive case is therefore bounded by execute_process(TIMEOUT)
# and asserted on the CAPTURED OUTPUT, never on the exit code (a timeout kill
# is not a failure). The negative case exits by itself, so its exit code is
# asserted.
#
# Written as a CMake script instead of shell because CI runs ctest on both
# Linux and Windows: no timeout(1), no bash, no output-redirection tricks.

cmake_minimum_required(VERSION 3.20)

foreach(var NODE_RUNNER FLOW_DIR)
    if(NOT DEFINED ${var} OR "${${var}}" STREQUAL "")
        message(FATAL_ERROR "${var} was not passed with -D — the check cannot run")
    endif()
endforeach()

# A loaded flow hangs until the timeout by design, so this bounds the test
# rather than racing it: startup to "flow loaded successfully" is <1 s, and
# even a loaded CI runner gets an order of magnitude of margin. This is the
# whole runtime of the positive case, hence the number's importance.
set(HL_TIMEOUT_SECONDS 10)

get_filename_component(RUNNER_DIR "${NODE_RUNNER}" DIRECTORY)

function(run_headless fixture out_log out_result)
    execute_process(
        COMMAND "${NODE_RUNNER}" --headless --run "${FLOW_DIR}/${fixture}"
                --log-console-enabled 1 --log-level Info
        WORKING_DIRECTORY "${RUNNER_DIR}"
        OUTPUT_VARIABLE stdout
        ERROR_VARIABLE stderr
        TIMEOUT ${HL_TIMEOUT_SECONDS}
        RESULT_VARIABLE result)
    set(${out_log} "${stdout}${stderr}" PARENT_SCOPE)
    set(${out_result} "${result}" PARENT_SCOPE)
endfunction()

function(dump_and_fail fixture log result reason)
    message("────────── ${fixture} (result: ${result}) ──────────")
    message("${log}")
    message(FATAL_ERROR "${reason}")
endfunction()

# ── AC4: every previously-unregistered component loads headless ──────────────
set(positive_flow "headless_all_components.flow")
if(NOT EXISTS "${FLOW_DIR}/${positive_flow}")
    message(FATAL_ERROR "fixture missing: ${FLOW_DIR}/${positive_flow}")
endif()

run_headless("${positive_flow}" positive_log positive_result)

if(NOT positive_log MATCHES "flow loaded successfully")
    dump_and_fail("${positive_flow}" "${positive_log}" "${positive_result}"
                  "AC4: the flow of previously-unregistered components did not load headless")
endif()

if(positive_log MATCHES "not supported in headless mode")
    dump_and_fail("${positive_flow}" "${positive_log}" "${positive_result}"
                  "AC4: a component that registerNodesHeadless() must provide was rejected")
endif()

# All six nodes, not merely some of them: a partial registration still loads
# the flow (unregistered nodes are skipped first), so the count is what pins
# the whole set.
if(NOT positive_log MATCHES "loaded nodes= 6 ")
    dump_and_fail("${positive_flow}" "${positive_log}" "${positive_result}"
                  "AC4: expected all 6 nodes to be instantiated, see log above")
endif()

# ── AC2: fail-fast survives for genuinely unknown types ──────────────────────
set(negative_flow "missing_node.flow")
if(NOT EXISTS "${FLOW_DIR}/${negative_flow}")
    message(FATAL_ERROR "fixture missing: ${FLOW_DIR}/${negative_flow}")
endif()

run_headless("${negative_flow}" negative_log negative_result)

if(NOT negative_result STREQUAL "1")
    dump_and_fail("${negative_flow}" "${negative_log}" "${negative_result}"
                  "AC2: expected exit code 1 for an unregistered node type")
endif()

# The message has to be VISIBLE, not merely emitted on a signal nobody
# connects (INC-015): the log must name the offending type.
if(NOT negative_log MATCHES "BogusUnregisteredNode")
    dump_and_fail("${negative_flow}" "${negative_log}" "${negative_result}"
                  "AC2: the fail-fast message does not name the unsupported type")
endif()

if(NOT negative_log MATCHES "not supported in headless mode")
    dump_and_fail("${negative_flow}" "${negative_log}" "${negative_result}"
                  "AC2: the fail-fast explanation is not printed to the log")
endif()

message(STATUS "headless registry check OK: "
               "6 components load, unknown type rejected with a named message")
