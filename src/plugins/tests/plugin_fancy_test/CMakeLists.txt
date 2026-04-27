# Use template system for plugin
create_plugin(plugin_fancy_test
    SOURCES
        PluginFancyInterface.cpp
        PluginFancyObject.cpp
        PluginFancyInterface.h
        PluginFancyObject.h
        fancy_test.qrc
    REQUIRES_LIBRARIES
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        frame_work
)
