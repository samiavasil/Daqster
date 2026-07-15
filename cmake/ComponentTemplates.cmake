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
    
    # Ensure the external library folder exists; optionally auto-init submodule
    set(EXTERNAL_DIR "${CMAKE_SOURCE_DIR}/src/plugins/external_libs/${COMPONENT_NAME}")
    if(NOT EXISTS "${EXTERNAL_DIR}")
        if(DEFINED DAQSTER_AUTO_INIT_SUBMODULES AND DAQSTER_AUTO_INIT_SUBMODULES)
            message(STATUS "External library '${COMPONENT_NAME}' missing - attempting to initialize submodule...")
            execute_process(
                COMMAND ${GIT_EXECUTABLE} submodule update --init -- "src/plugins/external_libs/${COMPONENT_NAME}"
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                RESULT_VARIABLE _git_res
                OUTPUT_QUIET ERROR_QUIET
            )
            if(NOT _git_res EQUAL 0)
                message(WARNING "Failed to init submodule for '${COMPONENT_NAME}' (git exit ${_git_res}). Please run: git submodule update --init src/plugins/external_libs/${COMPONENT_NAME}")
            endif()
        else()
            message(STATUS "External library '${COMPONENT_NAME}' not present (submodule not initialized). Skipping add_subdirectory.")
        endif()
    endif()

    if(EXISTS "${EXTERNAL_DIR}")
        # Add subdirectory - external library handles its own dependencies
        add_subdirectory(src/plugins/external_libs/${COMPONENT_NAME})

        # Register as available dependency
        register_external_library_dependency(${COMPONENT_NAME})

        # Append to global external libs list for meta-targets
        get_property(_externals GLOBAL PROPERTY EXTERNAL_LIBS_LIST)
        if(NOT _externals)
            set(_externals "")
        endif()
        list(APPEND _externals ${COMPONENT_NAME})
        set_property(GLOBAL PROPERTY EXTERNAL_LIBS_LIST "${_externals}")

        # Install/copy built artifact into main binary dir to aid detection/runtime
        # Attach a POST_BUILD copy only if the external project exposes a target
        if(TARGET ${COMPONENT_NAME})
            # Use a custom target that depends on the external target and performs the copy
            add_custom_target(copy_external_${COMPONENT_NAME}
                COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/lib
                COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:${COMPONENT_NAME}> ${CMAKE_BINARY_DIR}/lib
                DEPENDS ${COMPONENT_NAME}
                COMMENT "Copying external lib ${COMPONENT_NAME} -> ${CMAKE_BINARY_DIR}/lib"
            )
            # Make the main externals meta-target depend on this copy step
            get_property(_externals GLOBAL PROPERTY EXTERNAL_LIBS_LIST)
            if(_externals)
                add_dependencies(daqster_build_externals copy_external_${COMPONENT_NAME})
            endif()
        else()
            verbose_status("No same-named target for external '${COMPONENT_NAME}' - skipping automatic copy. If needed, build ${COMPONENT_NAME} and copy artifacts manually or add a wrapper target.")
        endif()

        message(STATUS "Created external library: ${COMPONENT_NAME}")
    endif()
endfunction()

# Create a convenience meta-target to build all registered external libraries
get_property(_registered_externals GLOBAL PROPERTY EXTERNAL_LIBS_LIST)
if(_registered_externals)
    add_custom_target(daqster_build_externals DEPENDS ${_registered_externals})
    message(STATUS "Added meta-target: daqster_build_externals (depends on: ${_registered_externals})")
else()
    # Target will be created later when externals are registered
    # (we can't create an empty target with no deps here)
endif()

