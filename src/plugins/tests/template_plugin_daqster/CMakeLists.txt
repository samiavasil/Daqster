# Use Qt detection from parent scope

add_library(plugin_template_test
    SHARED
    DaqsterTeplateInterface.cpp
    TemplatePluginObject.cpp
    DaqsterTeplateInterface.h
    TemplatePluginObject.h
    template.qrc
    DaqsterTeplateInterface.json
)

add_definitions(-DBUILD_AVAILABLE_PLUGIN)

target_include_directories(plugin_template_test
  PRIVATE
    ./
  )
target_link_libraries(plugin_template_test frame_work Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Gui)
