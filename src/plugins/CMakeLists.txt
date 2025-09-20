set(CMAKE_LIBRARY_OUTPUT_DIRECTORY  ${PROJECT_BINARY_DIR}/bin/plugins)

# Plugins - conditional based on Qt version, available modules and external libraries
message(STATUS "=== Plugins Status ===")

# Register plugins with their dependencies (from master branch analysis)
if(QT_VERSION_MAJOR EQUAL 5)
    # NodeEditor plugin - dependencies from master: Core, Gui, Multimedia, Charts, OpenGL
    register_plugin(NodeEditorPlugin
        REQUIRES_QT_MODULES Core Gui Multimedia Charts OpenGL
        REQUIRES_EXTERNAL_LIBS nodes
    )
    
    # QtCoinTrader plugin - dependencies from master: Core, Gui, Qml, QuickControls2, Charts, Network, OpenGL
    register_plugin(QtCoinTraderPlugin
        REQUIRES_QT_MODULES Core Gui Qml QuickControls2 Charts Network OpenGL
        REQUIRES_EXTERNAL_LIBS qtrest_lib
        REQUIRES_PACKAGES OpenSSL
    )
    
    # Test plugins - each plugin is independent
    register_plugin(PluginMainTest
        REQUIRES_QT_MODULES Core Gui
    )
    
    register_plugin(PluginFancyTest
        REQUIRES_QT_MODULES Core Gui
    )
    
    register_plugin(PluginUgglyTest
        REQUIRES_QT_MODULES Core Gui
    )
    
    register_plugin(TemplatePlugin
        REQUIRES_QT_MODULES Core Gui
    )
    
    # Add plugin subdirectories conditionally
    add_plugin_subdirectory(NodeEditorPlugin node_editor)
    add_plugin_subdirectory(QtCoinTraderPlugin QtCoinTrader)
    add_plugin_subdirectory(PluginMainTest tests/plugin_main_test)
    add_plugin_subdirectory(PluginFancyTest tests/plugin_fancy_test)
    add_plugin_subdirectory(PluginUgglyTest tests/plugin_uggly_test)
    add_plugin_subdirectory(TemplatePlugin tests/template_plugin_daqster)
    
else()
    # For Qt6, only enable basic test plugins (no external dependencies)
    register_plugin(PluginMainTest
        REQUIRES_QT_MODULES Core Gui
    )
    
    register_plugin(PluginFancyTest
        REQUIRES_QT_MODULES Core Gui
    )
    
    register_plugin(PluginUgglyTest
        REQUIRES_QT_MODULES Core Gui
    )
    
    register_plugin(TemplatePlugin
        REQUIRES_QT_MODULES Core Gui
    )
    
    add_plugin_subdirectory(PluginMainTest tests/plugin_main_test)
    add_plugin_subdirectory(PluginFancyTest tests/plugin_fancy_test)
    add_plugin_subdirectory(PluginUgglyTest tests/plugin_uggly_test)
    add_plugin_subdirectory(TemplatePlugin tests/template_plugin_daqster)
    message(STATUS "Only basic test plugins enabled for Qt6 - external library plugins disabled due to compatibility issues")
endif()

# Print final plugin status summary
print_plugin_status_summary()
