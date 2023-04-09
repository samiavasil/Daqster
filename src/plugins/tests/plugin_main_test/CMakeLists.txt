find_package(Qt5 COMPONENTS core gui REQUIRED)

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
