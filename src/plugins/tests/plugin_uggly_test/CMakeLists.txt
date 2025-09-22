# Use Qt detection from parent scope

add_library(plugin_uggly_test
    SHARED
    PluginUgglyInterface.cpp
    PluginUgglyObject.cpp
    PluginUgglyInterface.h
    PluginUgglyObject.h
    uggly_test.qrc
)

add_definitions(-DBUILD_AVAILABLE_PLUGIN)

target_include_directories(plugin_uggly_test
  PRIVATE
    ./
    ../../../frame_work/base/src/include
  )
# Automatic linking for all dependencies
link_component_dependencies(plugin_uggly_test)

# Install like other plugins
install(TARGETS plugin_uggly_test
    RUNTIME DESTINATION bin/plugins
    LIBRARY DESTINATION lib/daqster/plugins
    ARCHIVE DESTINATION lib
)

set_target_properties(plugin_uggly_test PROPERTIES
    INSTALL_RPATH "$ORIGIN/../../lib"
)
