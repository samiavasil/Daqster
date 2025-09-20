# PluginDependencyManager.cmake
# Manages plugin dependencies and conditional compilation based on external libraries

# Function to check if a plugin can be built based on its dependencies
function(check_plugin_dependencies PLUGIN_NAME)
    set(PLUGIN_ENABLED TRUE)
    set(REASONS "")
    
    # Parse arguments for dependencies
    set(options)
    set(oneValueArgs)
    set(multiValueArgs REQUIRES_QT_MODULES REQUIRES_EXTERNAL_LIBS REQUIRES_PACKAGES)
    cmake_parse_arguments(PLUGIN_DEPS "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    
    # Check Qt modules
    foreach(QT_MODULE ${PLUGIN_DEPS_REQUIRES_QT_MODULES})
        string(TOUPPER ${QT_MODULE} QT_MODULE_UPPER)
        set(QT_MODULE_VAR "QT_${QT_MODULE_UPPER}_LIB")
        
        # Special handling for QuickControls2
        if(QT_MODULE STREQUAL "QuickControls2")
            set(QT_MODULE_VAR "QT_QUICKCONTROLS2_LIB")
        endif()
        
        if(NOT ${QT_MODULE_VAR} OR "${${QT_MODULE_VAR}}" STREQUAL "")
            set(PLUGIN_ENABLED FALSE)
            list(APPEND REASONS "Qt${QT_VERSION_MAJOR}::${QT_MODULE} not available")
        endif()
    endforeach()
    
    # Check external libraries (targets)
    foreach(EXT_LIB ${PLUGIN_DEPS_REQUIRES_EXTERNAL_LIBS})
        if(NOT TARGET ${EXT_LIB})
            set(PLUGIN_ENABLED FALSE)
            list(APPEND REASONS "External library '${EXT_LIB}' not available")
        endif()
    endforeach()
    
    # Check packages
    foreach(PACKAGE ${PLUGIN_DEPS_REQUIRES_PACKAGES})
        string(TOUPPER ${PACKAGE} PACKAGE_UPPER)
        set(PACKAGE_VAR "${PACKAGE_UPPER}_FOUND")
        
        if(NOT ${PACKAGE_VAR})
            set(PLUGIN_ENABLED FALSE)
            list(APPEND REASONS "Package '${PACKAGE}' not found")
        endif()
    endforeach()
    
    # Set result variables in parent scope
    set(${PLUGIN_NAME}_ENABLED ${PLUGIN_ENABLED} PARENT_SCOPE)
    set(${PLUGIN_NAME}_REASONS "${REASONS}" PARENT_SCOPE)
    
    # Log result
    if(PLUGIN_ENABLED)
        message(STATUS "${PLUGIN_NAME} plugin: ENABLED")
    else()
        message(STATUS "${PLUGIN_NAME} plugin: DISABLED")
        foreach(REASON ${REASONS})
            message(STATUS "  - ${REASON}")
        endforeach()
    endif()
endfunction()

# Function to register a plugin with its dependencies
function(register_plugin PLUGIN_NAME)
    set(options)
    set(oneValueArgs)
    set(multiValueArgs REQUIRES_QT_MODULES REQUIRES_EXTERNAL_LIBS REQUIRES_PACKAGES)
    cmake_parse_arguments(PLUGIN_DEPS "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    
    # Check dependencies
    check_plugin_dependencies(${PLUGIN_NAME}
        REQUIRES_QT_MODULES ${PLUGIN_DEPS_REQUIRES_QT_MODULES}
        REQUIRES_EXTERNAL_LIBS ${PLUGIN_DEPS_REQUIRES_EXTERNAL_LIBS}
        REQUIRES_PACKAGES ${PLUGIN_DEPS_REQUIRES_PACKAGES}
    )
    
    # Store plugin info globally
    set_property(GLOBAL PROPERTY PLUGIN_${PLUGIN_NAME}_ENABLED ${${PLUGIN_NAME}_ENABLED})
    set_property(GLOBAL PROPERTY PLUGIN_${PLUGIN_NAME}_REASONS "${${PLUGIN_NAME}_REASONS}")
    
    # Add to global plugin list
    get_property(PLUGIN_NAMES GLOBAL PROPERTY PLUGIN_NAMES)
    if(NOT PLUGIN_NAMES)
        set(PLUGIN_NAMES "")
    endif()
    list(APPEND PLUGIN_NAMES ${PLUGIN_NAME})
    set_property(GLOBAL PROPERTY PLUGIN_NAMES "${PLUGIN_NAMES}")
endfunction()

# Function to check if a plugin is enabled
function(is_plugin_enabled PLUGIN_NAME RESULT_VAR)
    get_property(ENABLED GLOBAL PROPERTY PLUGIN_${PLUGIN_NAME}_ENABLED)
    set(${RESULT_VAR} ${ENABLED} PARENT_SCOPE)
endfunction()

# Function to get plugin disable reasons
function(get_plugin_reasons PLUGIN_NAME REASONS_VAR)
    get_property(REASONS GLOBAL PROPERTY PLUGIN_${PLUGIN_NAME}_REASONS)
    set(${REASONS_VAR} "${REASONS}" PARENT_SCOPE)
endfunction()

# Function to add plugin subdirectory conditionally
function(add_plugin_subdirectory PLUGIN_NAME PLUGIN_DIR)
    is_plugin_enabled(${PLUGIN_NAME} ENABLED)
    
    if(ENABLED)
        add_subdirectory(${PLUGIN_DIR})
        message(STATUS "Adding plugin subdirectory: ${PLUGIN_DIR}")
    else()
        get_plugin_reasons(${PLUGIN_NAME} REASONS)
        message(STATUS "Skipping plugin subdirectory: ${PLUGIN_DIR}")
        foreach(REASON ${REASONS})
            message(STATUS "  - ${REASON}")
        endforeach()
    endif()
endfunction()

# Function to check if external library can be built
function(check_external_library_buildability LIB_NAME)
    set(BUILDABLE TRUE)
    set(REASONS "")
    
    # Check if library directory exists
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/external_libs/${LIB_NAME}")
        set(BUILDABLE FALSE)
        list(APPEND REASONS "Library directory not found: src/external_libs/${LIB_NAME}")
    endif()
    
    # Check if CMakeLists.txt exists
    if(BUILDABLE AND NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/external_libs/${LIB_NAME}/CMakeLists.txt")
        set(BUILDABLE FALSE)
        list(APPEND REASONS "CMakeLists.txt not found in library directory")
    endif()
    
    # Set result variables in parent scope
    set(${LIB_NAME}_BUILDABLE ${BUILDABLE} PARENT_SCOPE)
    set(${LIB_NAME}_BUILD_REASONS "${REASONS}" PARENT_SCOPE)
endfunction()

# Function to print plugin status summary
function(print_plugin_status_summary)
    message(STATUS "=== Plugin Status Summary ===")
    
    # Get all registered plugins
    get_property(PLUGINS GLOBAL PROPERTY PLUGIN_NAMES)
    
    if(PLUGINS)
        foreach(PLUGIN ${PLUGINS})
            is_plugin_enabled(${PLUGIN} ENABLED)
            if(ENABLED)
                message(STATUS "✓ ${PLUGIN}: ENABLED")
            else()
                get_plugin_reasons(${PLUGIN} REASONS)
                message(STATUS "✗ ${PLUGIN}: DISABLED")
                foreach(REASON ${REASONS})
                    message(STATUS "    - ${REASON}")
                endforeach()
            endif()
        endforeach()
    else()
        message(STATUS "No plugins registered")
    endif()
endfunction()

# Function to print build configuration summary
function(print_build_configuration_summary)
    message(STATUS "=== Build Configuration Summary ===")
    message(STATUS "Qt Version: ${QT_VERSION_MAJOR}.${QT_VERSION_MINOR}.${QT_VERSION_PATCH}")
    message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
    message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
    message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
    
    # Check external library buildability
    check_external_library_buildability(nodeeditor)
    check_external_library_buildability(qtrest_lib)
    
    message(STATUS "External Libraries Buildability:")
    message(STATUS "  - nodeeditor: ${nodeeditor_BUILDABLE}")
    if(NOT nodeeditor_BUILDABLE)
        foreach(REASON ${nodeeditor_BUILD_REASONS})
            message(STATUS "    - ${REASON}")
        endforeach()
    endif()
    
    message(STATUS "  - qtrest_lib: ${qtrest_lib_BUILDABLE}")
    if(NOT qtrest_lib_BUILDABLE)
        foreach(REASON ${qtrest_lib_BUILD_REASONS})
            message(STATUS "    - ${REASON}")
        endforeach()
    endif()
endfunction()
