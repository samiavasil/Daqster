find_package(Qt5 COMPONENTS Core Gui REQUIRED)

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
  )
target_link_libraries(plugin_uggly_test frame_work Qt5::Core Qt5::Gui)
