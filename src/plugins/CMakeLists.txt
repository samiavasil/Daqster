set(CMAKE_LIBRARY_OUTPUT_DIRECTORY  ${PROJECT_BINARY_DIR}/bin/plugins)

# Plugins - conditional based on Qt version, available modules and external libraries
message(STATUS "=== Plugins Status ===")

# Register plugins with their dependencies (from master branch analysis)
if(QT_VERSION_MAJOR EQUAL 5)
    # Register plugins with their dependencies
    register_component(NodeEditorPlugin
        REQUIRES_LIBRARIES 
            Qt${QT_VERSION_MAJOR}::Core 
            Qt${QT_VERSION_MAJOR}::Gui 
            Qt${QT_VERSION_MAJOR}::Multimedia 
            Qt${QT_VERSION_MAJOR}::Charts 
            Qt${QT_VERSION_MAJOR}::OpenGL
            nodes
            frame_work
    )
    
    register_component(QtCoinTraderPlugin
        REQUIRES_LIBRARIES 
            Qt${QT_VERSION_MAJOR}::Core 
            Qt${QT_VERSION_MAJOR}::Gui 
            Qt${QT_VERSION_MAJOR}::Qml 
            Qt${QT_VERSION_MAJOR}::QuickControls2 
            Qt${QT_VERSION_MAJOR}::Charts 
            Qt${QT_VERSION_MAJOR}::Network 
            Qt${QT_VERSION_MAJOR}::OpenGL 
            Qt${QT_VERSION_MAJOR}::WebSockets
            qtrest_lib
            OpenSSL::SSL
            OpenSSL::Crypto
            frame_work
    )
    
    register_component(PluginMainTest
        REQUIRES_LIBRARIES 
            Qt${QT_VERSION_MAJOR}::Core 
            Qt${QT_VERSION_MAJOR}::Gui
            frame_work
    )
    
    register_component(PluginFancyTest
        REQUIRES_LIBRARIES 
            Qt${QT_VERSION_MAJOR}::Core 
            Qt${QT_VERSION_MAJOR}::Gui
            frame_work
    )
    
    register_component(PluginUgglyTest
        REQUIRES_LIBRARIES 
            Qt${QT_VERSION_MAJOR}::Core 
            Qt${QT_VERSION_MAJOR}::Gui
            frame_work
    )
    
    register_component(TemplatePlugin
        REQUIRES_LIBRARIES 
            Qt${QT_VERSION_MAJOR}::Core 
            Qt${QT_VERSION_MAJOR}::Gui
            frame_work
    )
    
    # Add plugin subdirectories conditionally
    add_component_subdirectory(NodeEditorPlugin node_editor)
    add_component_subdirectory(QtCoinTraderPlugin QtCoinTrader)
    add_component_subdirectory(PluginMainTest tests/plugin_main_test)
    add_component_subdirectory(PluginFancyTest tests/plugin_fancy_test)
    add_component_subdirectory(PluginUgglyTest tests/plugin_uggly_test)
    add_component_subdirectory(TemplatePlugin tests/template_plugin_daqster)
    
else()
    # For Qt6, only enable basic test plugins (no external dependencies)
    # Test plugins will register themselves in their own CMakeLists.txt files
    
    add_component_subdirectory(PluginMainTest tests/plugin_main_test)
    add_component_subdirectory(PluginFancyTest tests/plugin_fancy_test)
    add_component_subdirectory(PluginUgglyTest tests/plugin_uggly_test)
    add_component_subdirectory(TemplatePlugin tests/template_plugin_daqster)
    message(STATUS "Only basic test plugins enabled for Qt6 - external library plugins disabled due to compatibility issues")
endif()

# Print final plugin status summary
print_component_status_summary()
