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
    ../../../frame_work/base/src/include
  )
# Automatic linking for all dependencies
link_component_dependencies(plugin_template_test)
