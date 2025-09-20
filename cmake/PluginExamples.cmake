# PluginExamples.cmake
# Examples of how to register plugins with different dependency requirements

# Example 1: Simple plugin with only Qt modules
function(register_simple_plugin_example)
    register_plugin(SimplePlugin
        REQUIRES_QT_MODULES Core Widgets
    )
endfunction()

# Example 2: Plugin with external library dependency
function(register_external_lib_plugin_example)
    register_plugin(ExternalLibPlugin
        REQUIRES_QT_MODULES Core Widgets
        REQUIRES_EXTERNAL_LIBS some_external_lib
    )
endfunction()

# Example 3: Plugin with package dependency
function(register_package_plugin_example)
    register_plugin(PackagePlugin
        REQUIRES_QT_MODULES Core Network
        REQUIRES_PACKAGES OpenSSL
    )
endfunction()

# Example 4: Complex plugin with multiple dependencies
function(register_complex_plugin_example)
    register_plugin(ComplexPlugin
        REQUIRES_QT_MODULES Core Widgets Qml Quick Charts
        REQUIRES_EXTERNAL_LIBS qtrest_lib some_other_lib
        REQUIRES_PACKAGES OpenSSL
    )
endfunction()

# Example 5: Qt6 specific plugin
function(register_qt6_plugin_example)
    if(QT_VERSION_MAJOR EQUAL 6)
        register_plugin(Qt6SpecificPlugin
            REQUIRES_QT_MODULES Core Widgets
        )
    else()
        message(STATUS "Qt6SpecificPlugin: Skipped (not Qt6)")
    endif()
endfunction()

# Example 6: Conditional plugin based on build type
function(register_debug_plugin_example)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        register_plugin(DebugPlugin
            REQUIRES_QT_MODULES Core Widgets
        )
    else()
        message(STATUS "DebugPlugin: Skipped (not Debug build)")
    endif()
endfunction()

# Example 7: Plugin with custom dependency check
function(register_custom_dependency_plugin_example)
    # Custom check for specific condition
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/custom_lib")
        register_plugin(CustomDependencyPlugin
            REQUIRES_QT_MODULES Core
        )
    else()
        message(STATUS "CustomDependencyPlugin: Skipped (custom_lib not found)")
    endif()
endfunction()

# Example usage in CMakeLists.txt:
# 
# # Include examples
# include(PluginExamples)
# 
# # Register plugins
# register_simple_plugin_example()
# register_external_lib_plugin_example()
# register_package_plugin_example()
# register_complex_plugin_example()
# register_qt6_plugin_example()
# register_debug_plugin_example()
# register_custom_dependency_plugin_example()
# 
# # Add plugin subdirectories
# add_plugin_subdirectory(SimplePlugin src/plugins/simple)
# add_plugin_subdirectory(ExternalLibPlugin src/plugins/external_lib)
# add_plugin_subdirectory(PackagePlugin src/plugins/package)
# add_plugin_subdirectory(ComplexPlugin src/plugins/complex)
# add_plugin_subdirectory(Qt6SpecificPlugin src/plugins/qt6_specific)
# add_plugin_subdirectory(DebugPlugin src/plugins/debug)
# add_plugin_subdirectory(CustomDependencyPlugin src/plugins/custom)
