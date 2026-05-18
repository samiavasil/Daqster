# Use template system for plugin
create_plugin(plugin_uggly_test
    SOURCES
        PluginUgglyInterface.cpp
        PluginUgglyObject.cpp
        PluginUgglyInterface.h
        PluginUgglyObject.h
        uggly_test.qrc
    REQUIRES_LIBRARIES
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        frame_work
)
