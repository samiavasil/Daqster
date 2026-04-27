# Use template system for plugin
create_plugin(plugin_template_test
    SOURCES
        DaqsterTeplateInterface.cpp
        TemplatePluginObject.cpp
        DaqsterTeplateInterface.h
        TemplatePluginObject.h
        template.qrc
        DaqsterTeplateInterface.json
    REQUIRES_LIBRARIES
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        frame_work
)
