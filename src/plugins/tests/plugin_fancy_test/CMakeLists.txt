find_package(Qt5 COMPONENTS Core Gui REQUIRED)

add_library(plugin_fancy_test
    SHARED
    PluginFancyInterface.cpp
    PluginFancyObject.cpp
    PluginFancyInterface.h
    PluginFancyObject.h
    fancy_test.qrc
)

add_definitions(-DBUILD_AVAILABLE_PLUGIN)

target_include_directories(plugin_fancy_test
  PRIVATE
    ./
  )
target_link_libraries(plugin_fancy_test frame_work Qt5::Core Qt5::Gui)

# Install like other plugins
install(TARGETS plugin_fancy_test
    RUNTIME DESTINATION bin/plugins
    LIBRARY DESTINATION lib/daqster/plugins
    ARCHIVE DESTINATION lib
)

set_target_properties(plugin_fancy_test PROPERTIES
    INSTALL_RPATH "$ORIGIN/../../lib"
)
