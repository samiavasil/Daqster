# Plugin unit tests - one subdirectory per plugin under test.
#
# Mirror of src/plugins/. Add a subdirectory here when a plugin gains a
# test suite under tests/plugins/<plugin>/.

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/requirements_manager)
    add_subdirectory(requirements_manager)
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/demo_nodeditor_nodes)
    add_subdirectory(demo_nodeditor_nodes)
endif()
