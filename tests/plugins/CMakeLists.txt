# Plugin unit tests - one subdirectory per plugin under test.
#
# Mirror of src/plugins/. Add a subdirectory here when a plugin gains a
# test suite under tests/plugins/<plugin>/.

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/requirements_manager)
    add_subdirectory(requirements_manager)
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/nodeditor_nodes)
    add_subdirectory(nodeditor_nodes)
endif()

# REQ-SW-PL-050: ThreadOwner/ProcessOwner — the shared lifetime primitives in
# src/plugins/common/Threading/. One suite for the common library, next to the
# code it tests (mirrors src/plugins/).
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/common)
    add_subdirectory(common)
endif()

# REQ-SW-PL-053 Phase 2a: LayoutTree (QtCore schema-v2) + LayoutEngine
# (QtWidgets builder/capturer).
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/node_editor_ide)
    add_subdirectory(node_editor_ide)
endif()
