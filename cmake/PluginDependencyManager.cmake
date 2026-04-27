# PluginDependencyManager.cmake
# Manages plugin dependencies and conditional compilation based on external libraries

# Function to check if a plugin can be built based on its dependencies
function(check_plugin_dependencies PLUGIN_NAME)
    set(PLUGIN_ENABLED TRUE)
    set(REASONS "")
    
    # Parse arguments for dependencies
    set(options)
    set(oneValueArgs)
    set(multiValueArgs REQUIRES_LIBRARIES)
    cmake_parse_arguments(PLUGIN_DEPS "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    
    # Check all libraries (Qt modules, external libs, packages)
    foreach(LIBRARY ${PLUGIN_DEPS_REQUIRES_LIBRARIES})
        # Check if it's a Qt module
        if(LIBRARY MATCHES "^Qt[0-9]+::")
            # Extract module name from Qt5::ModuleName
            string(REGEX REPLACE "^Qt[0-9]+::" "" MODULE_NAME ${LIBRARY})
            
            # First try to find the module
            find_package(Qt${QT_VERSION_MAJOR}${MODULE_NAME} QUIET)
            
            # Check if target exists
            if(TARGET ${LIBRARY})
                message(STATUS "Checking ${LIBRARY} - target exists: TRUE")
            else()
                message(STATUS "Checking ${LIBRARY} - target exists: FALSE")
                set(PLUGIN_ENABLED FALSE)
                list(APPEND REASONS "${LIBRARY} not available")
            endif()
        else()
            # External library or package - check if target exists
            if(TARGET ${LIBRARY})
                message(STATUS "Checking ${LIBRARY} - target exists: TRUE")
            else()
                # Check if it's an external library that should be available
                get_property(IS_AVAILABLE GLOBAL PROPERTY EXTERNAL_LIB_${LIBRARY}_AVAILABLE)
                if(IS_AVAILABLE)
                    message(STATUS "Checking ${LIBRARY} - external library available: TRUE")
                else()
                    message(STATUS "Checking ${LIBRARY} - target exists: FALSE")
                    set(PLUGIN_ENABLED FALSE)
                    list(APPEND REASONS "${LIBRARY} not available")
                endif()
            endif()
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
function(register_component COMPONENT_NAME)
    set(options)
    set(oneValueArgs)
    set(multiValueArgs REQUIRES_LIBRARIES)
    cmake_parse_arguments(PLUGIN_DEPS "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    
    # Check dependencies
    check_plugin_dependencies(${COMPONENT_NAME}
        REQUIRES_LIBRARIES ${PLUGIN_DEPS_REQUIRES_LIBRARIES}
    )
    
    # Store component info globally
    set_property(GLOBAL PROPERTY COMPONENT_${COMPONENT_NAME}_ENABLED ${${COMPONENT_NAME}_ENABLED})
    set_property(GLOBAL PROPERTY COMPONENT_${COMPONENT_NAME}_REASONS "${${COMPONENT_NAME}_REASONS}")
    
    # Store dependencies for automatic linking
    set_property(GLOBAL PROPERTY COMPONENT_${COMPONENT_NAME}_LIBRARIES "${PLUGIN_DEPS_REQUIRES_LIBRARIES}")
    
    # Add to global component list
    get_property(COMPONENT_NAMES GLOBAL PROPERTY COMPONENT_NAMES)
    if(NOT COMPONENT_NAMES)
        set(COMPONENT_NAMES "")
    endif()
    list(APPEND COMPONENT_NAMES ${COMPONENT_NAME})
    set_property(GLOBAL PROPERTY COMPONENT_NAMES "${COMPONENT_NAMES}")
endfunction()

# Helper function to check if a component is enabled
function(is_component_enabled COMPONENT_NAME RESULT_VAR)
    get_property(ENABLED GLOBAL PROPERTY COMPONENT_${COMPONENT_NAME}_ENABLED)
    set(${RESULT_VAR} ${ENABLED} PARENT_SCOPE)
endfunction()

# Function to add component subdirectory conditionally
function(add_component_subdirectory COMPONENT_NAME COMPONENT_DIR)
    is_component_enabled(${COMPONENT_NAME} ENABLED)
    
    if(ENABLED)
        add_subdirectory(${COMPONENT_DIR})
        message(STATUS "Adding component subdirectory: ${COMPONENT_DIR}")
    else()
        get_property(REASONS GLOBAL PROPERTY COMPONENT_${COMPONENT_NAME}_REASONS)
        message(STATUS "Skipping component subdirectory: ${COMPONENT_DIR}")
        foreach(REASON ${REASONS})
            message(STATUS "  - ${REASON}")
        endforeach()
    endif()
endfunction()

# Function to automatically link component dependencies
function(link_component_dependencies COMPONENT_NAME)
    is_component_enabled(${COMPONENT_NAME} ENABLED)
    
    if(ENABLED)
        # Get stored dependencies
        get_property(LIBRARIES GLOBAL PROPERTY COMPONENT_${COMPONENT_NAME}_LIBRARIES)

        if(LIBRARIES)
            # Link each library only if available to avoid hard failures
            foreach(LIBRARY ${LIBRARIES})
                if(TARGET ${LIBRARY})
                    target_link_libraries(${COMPONENT_NAME} PRIVATE ${LIBRARY})
                    message(STATUS "Linked target ${LIBRARY} -> ${COMPONENT_NAME}")
                else()
                    # Check if it's a registered external library
                    get_property(IS_AVAILABLE GLOBAL PROPERTY EXTERNAL_LIB_${LIBRARY}_AVAILABLE)
                    if(IS_AVAILABLE)
                        # Attempt to link by name; the external lib may provide an imported target or name
                        target_link_libraries(${COMPONENT_NAME} PRIVATE ${LIBRARY})
                        message(STATUS "Linked external library ${LIBRARY} -> ${COMPONENT_NAME}")
                    else()
                        message(WARNING "Dependency '${LIBRARY}' for component '${COMPONENT_NAME}' not available; skipping link.\n  This may cause undefined references at link time if the dependency is really required.")
                    endif()
                endif()
            endforeach()
        else()
            message(STATUS "No automatic dependencies recorded for ${COMPONENT_NAME}")
        endif()

        message(STATUS "Auto-linked dependencies processed for ${COMPONENT_NAME}")
    endif()
endfunction()

# Function to register external library as available dependency
# This allows other components to find and use external libraries
function(register_external_library_dependency LIB_NAME)
    # Check if the library target exists (it should be built by add_subdirectory)
    if(TARGET ${LIB_NAME})
        message(STATUS "External library ${LIB_NAME} available as dependency")
        # Mark as available for other components
        set_property(GLOBAL PROPERTY EXTERNAL_LIB_${LIB_NAME}_AVAILABLE TRUE)
    else()
        message(STATUS "External library ${LIB_NAME} not available as dependency")
        set_property(GLOBAL PROPERTY EXTERNAL_LIB_${LIB_NAME}_AVAILABLE FALSE)
    endif()
endfunction()

# Function to print component status summary
function(print_component_status_summary)
    message(STATUS "=== Component Status Summary ===")
    
    # Get all registered components
    get_property(COMPONENTS GLOBAL PROPERTY COMPONENT_NAMES)
    
    if(COMPONENTS)
        foreach(COMPONENT ${COMPONENTS})
            is_component_enabled(${COMPONENT} ENABLED)
            if(ENABLED)
                message(STATUS "✓ ${COMPONENT}: ENABLED")
            else()
                get_property(REASONS GLOBAL PROPERTY COMPONENT_${COMPONENT}_REASONS)
                message(STATUS "✗ ${COMPONENT}: DISABLED")
                foreach(REASON ${REASONS})
                    message(STATUS "    - ${REASON}")
                endforeach()
            endif()
        endforeach()
    else()
        message(STATUS "No components registered")
    endif()
endfunction()

# Function to print build configuration summary
function(print_build_configuration_summary)
    message(STATUS "=== Build Configuration Summary ===")
    message(STATUS "Qt Version: ${QT_VERSION_MAJOR}.${QT_VERSION_MINOR}.${QT_VERSION_PATCH}")
    message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
    message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
    message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
endfunction()
