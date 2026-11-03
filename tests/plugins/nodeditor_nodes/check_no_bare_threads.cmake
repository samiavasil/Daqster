# check_no_bare_threads.cmake — REQ-SW-PL-050 regression guard.
#
# Rule: outside src/plugins/common/Threading/, code does not create a thread or
# a process by hand. It uses Daqster::ThreadOwner / Daqster::ProcessOwner
# (docs/Architecture/threading.md). The primitive exists because every
# hand-rolled copy of the pattern got one of the four steps wrong — a quit()
# without a wait(), a wait() without a deadline, a worker pointer used after the
# thread deleted it — and the failure only shows up as a crash on scene close,
# never at compile time.
#
# So the invariant is asserted statically: scan src/plugins/** (the Threading
# directory itself is the implementation and is excluded) for
#
#   new QThread | QThread::create | class ... : [public] QThread | new QProcess
#
# ALLOW_LIST below records an exception WITH a reason — never silently. Tests
# are not in src/plugins/, so they are outside this scan; they may create a
# bare thread/process only when they test the primitive itself (Rule 7).
#
# Usage:
#   cmake -DPLUGINS_DIR=<src/plugins> -P check_no_bare_threads.cmake
#
# Exits non-zero with a report when a non-allow-listed file creates one.

# ── Known exceptions ────────────────────────────────────────────────────────
# Format: "<path relative to PLUGINS_DIR>|<reason>". Empty on purpose: every
# QThread/QProcess in src/plugins/ belongs to the primitives' directory.
set(ALLOW_LIST
)

set(BANNED_PATTERNS
    "new[ \t\r\n]+QThread"
    "QThread::create"
    ":[ \t]*(public|private|protected)[ \t]+QThread"
    ":[ \t]*QThread[ \t\r\n]*[{]"
    "new[ \t\r\n]+QProcess"
)

# Human-readable names for the report (same order as BANNED_PATTERNS).
set(PATTERN_NAMES
    "new QThread"
    "QThread::create"
    "class : public QThread"
    "class : QThread"
    "new QProcess"
)

if(NOT DEFINED PLUGINS_DIR)
    message(FATAL_ERROR "PLUGINS_DIR not set")
endif()
if(NOT IS_DIRECTORY "${PLUGINS_DIR}")
    message(FATAL_ERROR "PLUGINS_DIR is not a directory: ${PLUGINS_DIR}")
endif()

# Allow-list as plain relative paths (list(FIND) below; no IN_LIST in `cmake -P`
# without CMP0057).
set(ALLOWED_PATHS)
foreach(ENTRY ${ALLOW_LIST})
    string(REPLACE "|" ";" PARTS "${ENTRY}")
    list(GET PARTS 0 PATH_PART)
    list(APPEND ALLOWED_PATHS "${PATH_PART}")
endforeach()

# ── Source collection ───────────────────────────────────────────────────────
file(GLOB_RECURSE PLUGIN_SOURCES
    "${PLUGINS_DIR}/*.cpp"
    "${PLUGINS_DIR}/*.cc"
    "${PLUGINS_DIR}/*.cxx"
    "${PLUGINS_DIR}/*.h"
    "${PLUGINS_DIR}/*.hpp"
)

# The implementation directory is the one place where a QThread is allowed to
# be created; everything else must go through it.
set(THREADING_DIR "${PLUGINS_DIR}/common/Threading")
set(SCANNED)
foreach(SOURCE ${PLUGIN_SOURCES})
    file(RELATIVE_PATH REL "${PLUGINS_DIR}" "${SOURCE}")
    string(REGEX MATCH "^common/Threading/" IN_THREADING "${REL}")
    if(NOT IN_THREADING)
        list(APPEND SCANNED "${SOURCE}")
    endif()
endforeach()

list(LENGTH SCANNED N_SCANNED)
if(N_SCANNED LESS 300)
    message(FATAL_ERROR
        "REQ-SW-PL-050: only ${N_SCANNED} sources collected under ${PLUGINS_DIR}.\n"
        "The real tree has 500+ plugin translation units — a scan this small means\n"
        "the GLOB broke. Refusing to run.")
endif()

set(VIOLATIONS "")
set(ALLOWED_HITS "")

foreach(SOURCE ${SCANNED})
    file(RELATIVE_PATH REL "${PLUGINS_DIR}" "${SOURCE}")
    file(READ "${SOURCE}" CONTENT)

    # Strip comments first: prose legitimately names the banned constructs
    # ("never call new QThread here"), and a comment is not a violation.
    string(REGEX REPLACE "//[^\r\n]*" "" CONTENT "${CONTENT}")
    string(REGEX REPLACE "/\\*([^*]|\\*+[^*/])*\\*+/" "" CONTENT "${CONTENT}")

    set(FOUND "")
    set(IDX 0)
    foreach(PATTERN ${BANNED_PATTERNS})
        string(REGEX MATCH "${PATTERN}" HIT "${CONTENT}")
        if(HIT)
            list(GET PATTERN_NAMES ${IDX} NAME)
            list(APPEND FOUND "${NAME}")
        endif()
        math(EXPR IDX "${IDX}+1")
    endforeach()

    if(FOUND)
        list(FIND ALLOWED_PATHS "${REL}" IS_ALLOWED)
        if(IS_ALLOWED EQUAL -1)
            string(REPLACE ";" ", " FOUND_TEXT "${FOUND}")
            list(APPEND VIOLATIONS "${REL}: ${FOUND_TEXT}")
        else()
            string(REPLACE ";" ", " FOUND_TEXT "${FOUND}")
            list(APPEND ALLOWED_HITS "${REL}: ${FOUND_TEXT}")
        endif()
    endif()
endforeach()

if(ALLOWED_HITS)
    message(STATUS "REQ-SW-PL-050 exceptions still present (allow-listed, not a failure):")
    foreach(HIT ${ALLOWED_HITS})
        message(STATUS "  - ${HIT}")
    endforeach()
endif()

if(VIOLATIONS)
    string(REPLACE ";" "\n  " DETAIL "${VIOLATIONS}")
    message(FATAL_ERROR
        "REQ-SW-PL-050 regression: a plugin creates a bare QThread/QProcess outside\n"
        "src/plugins/common/Threading/. Threads and processes have one owner in this tree:\n"
        "  Daqster::ThreadOwner<Worker>  (QThread + moveToThread + join with a deadline)\n"
        "  Daqster::ProcessOwner         (terminate -> grace -> kill, own/adopted)\n"
        "Rules: docs/Architecture/threading.md. If the file genuinely must create one\n"
        "(the primitives' own implementation, or a test living under src/plugins/),\n"
        "add it to ALLOW_LIST in check_no_bare_threads.cmake WITH a reason.\n"
        "Offending files:\n  ${DETAIL}")
endif()

message(STATUS "no_bare_threads: ${N_SCANNED} sources scanned under ${PLUGINS_DIR}, 0 bare QThread/QProcess")
