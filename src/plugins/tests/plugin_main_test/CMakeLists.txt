find_package(Qt5 COMPONENTS Core Gui REQUIRED)

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
  )
target_link_libraries(plugin_main_test frame_work Qt5::Core Qt5::Gui)

# Install like other plugins
install(TARGETS plugin_main_test
    RUNTIME DESTINATION bin/plugins
    LIBRARY DESTINATION lib/daqster/plugins
    ARCHIVE DESTINATION lib
)

set_target_properties(plugin_main_test PROPERTIES
    INSTALL_RPATH "$ORIGIN/../../lib"
)
