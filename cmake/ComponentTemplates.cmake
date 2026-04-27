# Component Templates for Daqster
# Provides standardized templates for different component types

# Template for Applications
function(create_application COMPONENT_NAME)
    set(options)
    set(oneValueArgs)
    set(multiValueArgs REQUIRES_LIBRARIES SOURCES)
    cmake_parse_arguments(APP "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    
    # Register as component and check dependencies FIRST
    register_component(${COMPONENT_NAME}
        REQUIRES_LIBRARIES ${APP_REQUIRES_LIBRARIES}
    )
    
    # Check if component is enabled
    get_property(COMPONENT_ENABLED GLOBAL PROPERTY COMPONENT_${COMPONENT_NAME}_ENABLED)
    
    if(NOT COMPONENT_ENABLED)
        get_property(REASONS GLOBAL PROPERTY COMPONENT_${COMPONENT_NAME}_REASONS)
        message(STATUS "Skipping application ${COMPONENT_NAME} - dependencies not met:")
        foreach(REASON ${REASONS})
            message(STATUS "  - ${REASON}")
        endforeach()
        return()  # Exit early if dependencies not met
    endif()
    
    # Create executable ONLY if dependencies are met
    add_executable(${COMPONENT_NAME} ${APP_SOURCES})
    
    # Auto-link dependencies
    link_component_dependencies(${COMPONENT_NAME})
    
    # Install executable
    install(TARGETS ${COMPONENT_NAME}
        RUNTIME DESTINATION bin
    )
    
    message(STATUS "Created application: ${COMPONENT_NAME}")
endfunction()

# Template for Internal Libraries
function(create_internal_library COMPONENT_NAME)
    set(options)
    set(oneValueArgs)
    set(multiValueArgs REQUIRES_LIBRARIES SOURCES)
    cmake_parse_arguments(LIB "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    
    # Register as component and check dependencies FIRST
    register_component(${COMPONENT_NAME}
        REQUIRES_LIBRARIES ${LIB_REQUIRES_LIBRARIES}
    )
    
    # Check if component is enabled
    get_property(COMPONENT_ENABLED GLOBAL PROPERTY COMPONENT_${COMPONENT_NAME}_ENABLED)
    
    if(NOT COMPONENT_ENABLED)
        get_property(REASONS GLOBAL PROPERTY COMPONENT_${COMPONENT_NAME}_REASONS)
        message(STATUS "Skipping library ${COMPONENT_NAME} - dependencies not met:")
        foreach(REASON ${REASONS})
            message(STATUS "  - ${REASON}")
        endforeach()
        return()  # Exit early if dependencies not met
    endif()
    
    # Create library ONLY if dependencies are met
    add_library(${COMPONENT_NAME} SHARED ${LIB_SOURCES})
    
    # Auto-link dependencies
    link_component_dependencies(${COMPONENT_NAME})
    
    # Install library
    install(TARGETS ${COMPONENT_NAME}
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
    )
    
    message(STATUS "Created internal library: ${COMPONENT_NAME}")
endfunction()

# Template for Plugins (including test plugins)
function(create_plugin COMPONENT_NAME)
    set(options)
    set(oneValueArgs INSTALL_RPATH)
    set(multiValueArgs REQUIRES_LIBRARIES SOURCES INCLUDE_DIRECTORIES COMPILE_DEFINITIONS LINK_LIBRARIES)
    cmake_parse_arguments(PLUGIN "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    
    # Register as component and check dependencies FIRST
    register_component(${COMPONENT_NAME}
        REQUIRES_LIBRARIES ${PLUGIN_REQUIRES_LIBRARIES}
    )
    
    # Check if component is enabled
    get_property(COMPONENT_ENABLED GLOBAL PROPERTY COMPONENT_${COMPONENT_NAME}_ENABLED)
    
    if(NOT COMPONENT_ENABLED)
        get_property(REASONS GLOBAL PROPERTY COMPONENT_${COMPONENT_NAME}_REASONS)
        message(STATUS "Skipping plugin ${COMPONENT_NAME} - dependencies not met:")
        foreach(REASON ${REASONS})
            message(STATUS "  - ${REASON}")
        endforeach()
        return()  # Exit early if dependencies not met
    endif()
    
    # Create plugin library ONLY if dependencies are met
    add_library(${COMPONENT_NAME} SHARED ${PLUGIN_SOURCES})
    
    # Add plugin definition - CRITICAL for PLUGIN_EXPORT to work
    target_compile_definitions(${COMPONENT_NAME} PRIVATE BUILD_AVAILABLE_PLUGIN)
    
    # Standard include directories for all plugins
    target_include_directories(${COMPONENT_NAME}
        PRIVATE
            ./
            ../../../frame_work/base/src/include
    )
    
    # Additional include directories if specified
    if(PLUGIN_INCLUDE_DIRECTORIES)
        target_include_directories(${COMPONENT_NAME} PRIVATE ${PLUGIN_INCLUDE_DIRECTORIES})
    endif()
    
    # Additional compile definitions if specified
    if(PLUGIN_COMPILE_DEFINITIONS)
        target_compile_definitions(${COMPONENT_NAME} PRIVATE ${PLUGIN_COMPILE_DEFINITIONS})
    endif()
    
    # Additional link libraries if specified
    if(PLUGIN_LINK_LIBRARIES)
        target_link_libraries(${COMPONENT_NAME} PRIVATE ${PLUGIN_LINK_LIBRARIES})
    endif()
    
    # Auto-link dependencies
    link_component_dependencies(${COMPONENT_NAME})
    
    # Install plugin
    install(TARGETS ${COMPONENT_NAME}
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION bin
        ARCHIVE DESTINATION lib
    )
    
    # Set RPATH for plugins (use custom or default)
    if(PLUGIN_INSTALL_RPATH)
        set_target_properties(${COMPONENT_NAME} PROPERTIES
            INSTALL_RPATH "${PLUGIN_INSTALL_RPATH}"
        )
    else()
        set_target_properties(${COMPONENT_NAME} PROPERTIES
            INSTALL_RPATH "$ORIGIN/../../lib"
        )
    endif()
    
    message(STATUS "Created plugin: ${COMPONENT_NAME}")
endfunction()

# Template for External Libraries
function(create_external_library COMPONENT_NAME)
    set(options)
    set(oneValueArgs)
    set(multiValueArgs)
    cmake_parse_arguments(EXT_LIB "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    
    # Add subdirectory - external library handles its own dependencies
    add_subdirectory(src/external_libs/${COMPONENT_NAME})
    
    # Register as available dependency
    register_external_library_dependency(${COMPONENT_NAME})
    
    message(STATUS "Created external library: ${COMPONENT_NAME}")
endfunction()

