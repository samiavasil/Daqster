# Use template system for plugin
create_plugin(plugin_main_test
    SOURCES
        PluginMainInterface.cpp
        PluginMainObject.cpp
        PluginMainInterface.h
        PluginMainObject.h
        main_test.qrc
    REQUIRES_LIBRARIES
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        frame_work
)
