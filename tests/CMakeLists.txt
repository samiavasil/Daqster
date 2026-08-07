# Central unit-test orchestrator.
#
# The tests/ tree mirrors the src/ layout (apps/, framework/, plugins/).
# Each family subdirectory is added only when it exists, so this tree stays
# compatible with builds that do not ship a given family.
#
# Tests are gated behind the DAQSTER_BUILD_TESTS option (default OFF) in the
# root CMakeLists.txt. We deliberately do NOT use include(CTest)/BUILD_TESTING
# here because nodeeditor defines its own BUILD_TESTING option and enabling it
# would trigger its Catch2 dependency download.

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/apps)
    add_subdirectory(apps)
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/framework)
    add_subdirectory(framework)
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/plugins)
    add_subdirectory(plugins)
endif()
