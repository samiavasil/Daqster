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
# Timeouts here are budgets, not guesses (see "Time budgets" below): a hard 10 s
# cap killed the negative case mid-PulseAudio-stall and reported AC2 as broken
# while the flow had been rejected correctly.
#
# Written as a CMake script instead of shell because CI runs ctest on both
# Linux and Windows: no timeout(1), no bash, no output-redirection tricks.

cmake_minimum_required(VERSION 3.20)

foreach(var NODE_RUNNER FLOW_DIR)
    if(NOT DEFINED ${var} OR "${${var}}" STREQUAL "")
        message(FATAL_ERROR "${var} was not passed with -D — the check cannot run")
    endif()
endforeach()

# ── Time budgets ────────────────────────────────────────────────────────────
# Qt Multimedia's connect to PulseAudio/PipeWire has its OWN long timeout.
# While the audio device is held — by the SIGTERM'd NodeRunner of a previous
# run, by another binary without the lock, by a damaged server — the child
# prints NOTHING for that long and only then starts working. Measured
# 2026-11-14 on this machine: a healthy rejection costs 0.46–0.56 s, while the
# same rejection next to an audio-holding NodeRunner cost 30.66 s, its log
# reading `Using Qt multimedia` … 30.3 s later … `PulseAudioService: Connection
# failure: "Timeout"`. (Qt's nominal timeout is 25 s; the observed value is what
# the budget must cover, hence 35.)
#
# A fixed 10 s cap therefore does not bound a slow start, it truncates a stall:
# the child is killed before it can reject the flow, and AC2 reports "expected
# exit code 1" for a flow that was in fact rejected. The numbers below are the
# stall plus the work that follows it, and both are named so the reason travels
# with the value.
set(HL_PULSE_STALL_SECONDS 35)  # Qt Multimedia's PulseAudio connect: measured 30.3 s
set(HL_PROCESSING_SECONDS 10)   # plugin load + flow parsing/registration after it
set(HL_SETTLE_SECONDS 10)       # healthy run: startup to verdict is well under 1 s
math(EXPR HL_STALLED_TIMEOUT_SECONDS
     "${HL_PULSE_STALL_SECONDS} + ${HL_PROCESSING_SECONDS}")

get_filename_component(RUNNER_DIR "${NODE_RUNNER}" DIRECTORY)

function(run_headless fixture out_log out_result timeout_seconds)
    execute_process(
        COMMAND "${NODE_RUNNER}" --headless --run "${FLOW_DIR}/${fixture}"
                --log-console-enabled 1 --log-level Info
        WORKING_DIRECTORY "${RUNNER_DIR}"
        OUTPUT_VARIABLE stdout
        ERROR_VARIABLE stderr
        TIMEOUT ${timeout_seconds}
        RESULT_VARIABLE result)
    set(${out_log} "${stdout}${stderr}" PARENT_SCOPE)
    set(${out_result} "${result}" PARENT_SCOPE)
endfunction()

# A fixture that exits by itself gets the full stall budget outright: TIMEOUT is
# a ceiling, not a wait, so a healthy rejection still costs its 0.5 s and only a
# stall spends the rest.
function(run_headless_to_exit fixture out_log out_result)
    run_headless("${fixture}" log result ${HL_STALLED_TIMEOUT_SECONDS})
    set(${out_log} "${log}" PARENT_SCOPE)
    set(${out_result} "${result}" PARENT_SCOPE)
endfunction()

# A fixture that never exits is stopped by its own TIMEOUT, so the budget IS the
# runtime: waiting the stall budget on every healthy run would cost 35 s per
# ctest run instead of 10. The second window is spent only when the first one
# produced none of the expected output — the stall is invisible in the log until
# it ends, so "no marker yet" is the only condition available, and it is a
# condition (asserted output) rather than a longer number.
function(run_headless_until fixture marker out_log out_result)
    run_headless("${fixture}" log result ${HL_SETTLE_SECONDS})
    if(NOT log MATCHES "${marker}")
        message(STATUS "${fixture}: no '${marker}' within ${HL_SETTLE_SECONDS}s — "
                       "retrying with the ${HL_STALLED_TIMEOUT_SECONDS}s budget "
                       "(audio stack may have been holding the device)")
        run_headless("${fixture}" log result ${HL_STALLED_TIMEOUT_SECONDS})
    endif()
    set(${out_log} "${log}" PARENT_SCOPE)
    set(${out_result} "${result}" PARENT_SCOPE)
endfunction()

function(dump_and_fail fixture log result reason)
    message("────────── ${fixture} (result: ${result}) ──────────")
    message("${log}")
    if(result MATCHES "timeout")
        message("The child did not finish inside its budget "
                "(${HL_SETTLE_SECONDS}s normal / ${HL_STALLED_TIMEOUT_SECONDS}s "
                "after a PulseAudio stall). Prime suspect is the audio stack, not "
                "the flow: Qt Multimedia blocks ~25–31 s on its own connect when the "
                "device is held (measured 30.3 s), and the log then carries "
                "`PulseAudioService: Connection failure` before any flow message. "
                "Check RESOURCE_LOCK audio on this test and on every audio binary "
                "that ran alongside it.")
    endif()
    message(FATAL_ERROR "${reason}")
endfunction()


# ── AC2: fail-fast survives for genuinely unknown types ──────────────────────
# Run this FIRST: the negative case exits cleanly (exit code 1) and does not
# leave audio resources in a dirty state. The positive case (below) includes
# AudioDisplayObsolete nodes that monitor audio format changes; when cmake's
# execute_process TIMEOUT kills it via SIGTERM, the abrupt termination can
# leave PulseAudio/PipeWire in a state that hangs a subsequent run.
set(negative_flow "missing_node.flow")
if(NOT EXISTS "${FLOW_DIR}/${negative_flow}")
    message(FATAL_ERROR "fixture missing: ${FLOW_DIR}/${negative_flow}")
endif()

run_headless_to_exit("${negative_flow}" negative_log negative_result)

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

# ── AC4: every previously-unregistered component loads headless ──────────────
set(positive_flow "headless_all_components.flow")
if(NOT EXISTS "${FLOW_DIR}/${positive_flow}")
    message(FATAL_ERROR "fixture missing: ${FLOW_DIR}/${positive_flow}")
endif()

run_headless_until("${positive_flow}" "flow loaded successfully"
                   positive_log positive_result)

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

message(STATUS "headless registry check OK: "
               "6 components load, unknown type rejected with a named message")
