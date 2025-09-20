set(CMAKE_LIBRARY_OUTPUT_DIRECTORY  ${PROJECT_BINARY_DIR}/bin/plugins)

# Plugins - conditional based on Qt version, available modules and external libraries
message(STATUS "=== Plugins Status ===")

# Register plugins with their dependencies
if(QT_VERSION_MAJOR EQUAL 5)
    # NodeEditor plugin
    register_plugin(NodeEditorPlugin
        REQUIRES_QT_MODULES Multimedia Widgets
        REQUIRES_EXTERNAL_LIBS nodes
    )
    
    # QtCoinTrader plugin
    register_plugin(QtCoinTraderPlugin
        REQUIRES_QT_MODULES Qml Quick QuickControls2 Charts Network
        REQUIRES_EXTERNAL_LIBS qtrest_lib
        REQUIRES_PACKAGES OpenSSL
    )
    
    # Test plugins (always available)
    register_plugin(TestPlugins
        REQUIRES_QT_MODULES Core Widgets
    )
    
    # Add plugin subdirectories conditionally
    add_plugin_subdirectory(NodeEditorPlugin node_editor)
    add_plugin_subdirectory(QtCoinTraderPlugin QtCoinTrader)
    add_plugin_subdirectory(TestPlugins tests)
    
else()
    # For Qt6, only enable basic test plugins (no external dependencies)
    register_plugin(TestPlugins
        REQUIRES_QT_MODULES Core Widgets
    )
    
    add_plugin_subdirectory(TestPlugins tests)
    message(STATUS "Only basic test plugins enabled for Qt6 - external library plugins disabled due to compatibility issues")
endif()

# Print final plugin status summary
print_plugin_status_summary()
