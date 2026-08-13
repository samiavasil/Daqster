# Framework unit tests - one subdirectory per framework component under test.
#
# Mirror of src/frame_work/. Add a subdirectory here when a component gains a
# test suite under tests/framework/<component>/.

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/perf)
    add_subdirectory(perf)
endif()
