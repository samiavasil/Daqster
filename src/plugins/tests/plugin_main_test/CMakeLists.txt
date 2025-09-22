# Use Qt detection from parent scope

add_library(plugin_main_test
    SHARED
    PluginMainInterface.cpp
    PluginMainObject.cpp
    PluginMainInterface.h
    PluginMainObject.h
    main_test.qrc
)

add_definitions(-DBUILD_AVAILABLE_PLUGIN)

target_include_directories(plugin_main_test
  PRIVATE
    ./
    ../../../frame_work/base/src/include
  )
# Automatic linking for all dependencies
link_component_dependencies(plugin_main_test)

# Install like other plugins
install(TARGETS plugin_main_test
    RUNTIME DESTINATION bin/plugins
    LIBRARY DESTINATION lib/daqster/plugins
    ARCHIVE DESTINATION lib
)

set_target_properties(plugin_main_test PROPERTIES
    INSTALL_RPATH "$ORIGIN/../../lib"
)
