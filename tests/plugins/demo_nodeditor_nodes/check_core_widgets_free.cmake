# check_core_widgets_free.cmake — REQ-SW-PL-051 AC3/AC7 regression guard.
#
# The core/gui split is a *build-graph* property, not a runtime one: the
# `_core` plugin must not name a QtWidgets class, because QtWidgets being on
# its link line is exactly what makes a "core" node unusable in a headless
# process. Nothing in the C++ type system enforces that, and nothing in ctest
# would notice a widget creeping back in — a new `#include <QSlider>` in a
# model compiles, links and passes every existing test, and the split silently
# regresses to "core with widgets".
#
# So the invariant is asserted here, by scanning the _core source tree for
# QtWidgets header includes. Header include is the right tripwire rather than
# class-name usage: a model legitimately forward-declares `class QWidget;` and
# returns `QWidget*` from `embeddedWidget()` (REQ-SW-PL-051's whole point), and
# that must NOT fail. Including <QWidget> or <QStackedWidget> means the
# translation unit needs the real definition, i.e. it will not link headless.
#
# ALLOW_LIST below is the recorded deferral list from REQ-SW-PL-051 AC2 — the
# two TUs still pulling widgets into _core. When one is moved, delete its entry
# here; the test then guards the direction of travel instead of silently
# tolerating whatever is present today.
#
# Usage: cmake -DCORE_DIR=<path to demo_nodeditor_nodes_core> -P check_core_widgets_free.cmake
#
# Exits non-zero with a report on stderr when a non-allow-listed TU includes a
# widgets header.

# ── Known deferrals (REQ-SW-PL-051 AC2) ──────────────────────────────────────
set(ALLOW_LIST
    "Displays/AudioDisplay/AudioDisplayModelObsolete.cpp|QStackedWidget — dead include: the TU contains no `new` at all (verified 2026-10-08), so nothing is constructed and the include only has to go; deferred with the obsolete component itself"
    "Sources/Video/VideoEffectNode.cpp|QLabel — effect preview label still constructed by the model; scheduled for NodeWidgetFactory"
)

# QtWidgets public headers that, when included from _core, put the widget
# dependency back on the link line. QWidget itself is the root of the module.
set(WIDGET_HEADERS
    QtWidgets
    QWidget
    QApplication
    QWidgetItem
    QLayout
    QBoxLayout
    QVBoxLayout
    QHBoxLayout
    QGridLayout
    QFormLayout
    QStackedLayout
    QStackedWidget
    QTabWidget
    QSplitter
    QDockWidget
    QMainWindow
    QDialog
    QDialogButtonBox
    QMessageBox
    QFileDialog
    QColorDialog
    QFontDialog
    QLabel
    QPushButton
    QToolButton
    QRadioButton
    QCheckBox
    QComboBox
    QSpinBox
    QDoubleSpinBox
    QSlider
    QDial
    QProgressBar
    QGroupBox
    QScrollArea
    QScrollBar
    QLineEdit
    QTextEdit
    QPlainTextEdit
    QAbstractItemView
    QTableView
    QTableWidget
    QTreeView
    QTreeWidget
    QListView
    QListWidget
    QSizePolicy
    QStyle
    QPalette
    QAction
    QMenu
    QMenuBar
    QToolBar
    QStatusBar
    QMovie
    QVideoWidget
)

if(NOT DEFINED CORE_DIR)
    message(FATAL_ERROR "CORE_DIR not set")
endif()
if(NOT IS_DIRECTORY "${CORE_DIR}")
    message(FATAL_ERROR "CORE_DIR is not a directory: ${CORE_DIR}")
endif()

# list(FIND ...) rather than `if(X IN_LIST Y)`: this runs as a `cmake -P`
# script, where CMP0057 is not set by default and the IN_LIST operator would
# be interpreted as three plain arguments.
cmake_policy(SET CMP0057 NEW)

# Allow-list as a plain list of relative paths for lookup.
set(ALLOWED_PATHS)
foreach(ENTRY ${ALLOW_LIST})
    string(REPLACE "|" ";" PARTS "${ENTRY}")
    list(GET PARTS 0 PATH_PART)
    list(APPEND ALLOWED_PATHS "${PATH_PART}")
endforeach()

# Collect every source file under _core.
file(GLOB_RECURSE CORE_SOURCES
    "${CORE_DIR}/*.cpp"
    "${CORE_DIR}/*.h"
    "${CORE_DIR}/*.hpp"
)

set(VIOLATIONS "")
set(ALLOWED_HITS "")

# Instead of one gigantic alternation (CMake's regex engine rejects it), pull
# every #include <...> out of the file once and compare the module names
# against the list above. One regex pass per file instead of 60.
set(FOUND_WIDGETS "")
foreach(SOURCE ${CORE_SOURCES})
    file(RELATIVE_PATH REL "${CORE_DIR}" "${SOURCE}")
    file(READ "${SOURCE}" CONTENT)
    string(REGEX MATCHALL "#include[ \t]*<[^>]*>" RAW_INCLUDES "${CONTENT}")
    foreach(RAW ${RAW_INCLUDES})
        # "<QtWidgets/QPushButton>" / "<QWidget>" / "<QWidgetItem>" → module name
        string(REGEX REPLACE "^#include[ \t]*<" "" MODULE "${RAW}")
        string(REGEX REPLACE ">$" "" MODULE "${MODULE}")
        string(REGEX REPLACE "/.*$" "" MODULE "${MODULE}")
        string(REGEX REPLACE "\\.(h|hpp|hxx)$" "" MODULE "${MODULE}")
        list(FIND WIDGET_HEADERS "${MODULE}" HIT)
        if(NOT HIT EQUAL -1)
            list(FIND FOUND_WIDGETS "${MODULE}" SEEN)
            if(SEEN EQUAL -1)
                list(APPEND FOUND_WIDGETS "${MODULE}")
            endif()
        endif()
    endforeach()

    if(FOUND_WIDGETS)
        list(SORT FOUND_WIDGETS)
        list(FIND ALLOWED_PATHS "${REL}" IS_ALLOWED)
        if(IS_ALLOWED EQUAL -1)
            list(APPEND VIOLATIONS "${REL}: ${FOUND_WIDGETS}")
        else()
            list(APPEND ALLOWED_HITS "${REL}: ${FOUND_WIDGETS}")
        endif()
        set(FOUND_WIDGETS "")
    endif()
endforeach()

if(ALLOWED_HITS)
    message(STATUS "REQ-SW-PL-051 AC2 deferrals still present (allow-listed, not a failure):")
    foreach(HIT ${ALLOWED_HITS})
        message(STATUS "  - ${HIT}")
    endforeach()
endif()

if(VIOLATIONS)
    string(REPLACE ";" "\n  " DETAIL "${VIOLATIONS}")
    message(FATAL_ERROR
        "REQ-SW-PL-051 regression: ${CORE_DIR} includes QtWidgets outside the recorded deferral list.\n"
        "The core plugin is supposed to link without QtWidgets so it can be loaded by a\n"
        "headless runtime. Move the widget to demo_nodeditor_nodes_gui and reach it through\n"
        "NodeWidgetFactory / IWidgetProvider, or — if it genuinely belongs to a model that is\n"
        "scheduled for removal — add it to ALLOW_LIST in check_core_widgets_free.cmake with a\n"
        "reason, so the deferral stays recorded instead of becoming invisible.\n"
        "Offending translation units:\n  ${DETAIL}")
endif()

list(LENGTH CORE_SOURCES N)
message(STATUS "core_is_widgets_free: ${N} sources scanned in ${CORE_DIR}, 0 undeclared QtWidgets includes")