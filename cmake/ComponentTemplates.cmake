# Component Templates for Daqster
# Provides standardized templates for different component types

# Template for Applications
function(create_application COMPONENT_NAME)
    set(options)
    set(oneValueArgs)
    set(multiValueArgs REQUIRES_LIBRARIES SOURCES)
    cmake_parse_arguments(APP "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    
    # Create executable
    add_executable(${COMPONENT_NAME} ${APP_SOURCES})
    
    # Register as component
    register_component(${COMPONENT_NAME}
        REQUIRES_LIBRARIES ${APP_REQUIRES_LIBRARIES}
    )
    
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
    
    # Create library
    add_library(${COMPONENT_NAME} SHARED ${LIB_SOURCES})
    
    # Register as component
    register_component(${COMPONENT_NAME}
        REQUIRES_LIBRARIES ${LIB_REQUIRES_LIBRARIES}
    )
    
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

# Template for Plugins
function(create_plugin COMPONENT_NAME)
    set(options)
    set(oneValueArgs)
    set(multiValueArgs REQUIRES_LIBRARIES SOURCES)
    cmake_parse_arguments(PLUGIN "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    
    # Create plugin library
    add_library(${COMPONENT_NAME} SHARED ${PLUGIN_SOURCES})
    
    # Add plugin definition
    target_compile_definitions(${COMPONENT_NAME} PRIVATE BUILD_AVAILABLE_PLUGIN)
    
    # Register as component
    register_component(${COMPONENT_NAME}
        REQUIRES_LIBRARIES ${PLUGIN_REQUIRES_LIBRARIES}
    )
    
    # Auto-link dependencies
    link_component_dependencies(${COMPONENT_NAME})
    
    # Install plugin
    install(TARGETS ${COMPONENT_NAME}
        RUNTIME DESTINATION bin/plugins
        LIBRARY DESTINATION lib/daqster/plugins
        ARCHIVE DESTINATION lib
    )
    
    # Set RPATH for plugins
    set_target_properties(${COMPONENT_NAME} PROPERTIES
        INSTALL_RPATH "$ORIGIN/../../lib"
    )
    
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

# Template for Test Components
function(create_test_component COMPONENT_NAME)
    set(options)
    set(oneValueArgs)
    set(multiValueArgs REQUIRES_LIBRARIES SOURCES)
    cmake_parse_arguments(TEST "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    
    # Create test plugin
    add_library(${COMPONENT_NAME} SHARED ${TEST_SOURCES})
    
    # Add plugin definition
    target_compile_definitions(${COMPONENT_NAME} PRIVATE BUILD_AVAILABLE_PLUGIN)
    
    # Include directories
    target_include_directories(${COMPONENT_NAME}
        PRIVATE
            ./
            ../../../frame_work/base/src/include
    )
    
    # Register as component
    register_component(${COMPONENT_NAME}
        REQUIRES_LIBRARIES ${TEST_REQUIRES_LIBRARIES}
    )
    
    # Auto-link dependencies
    link_component_dependencies(${COMPONENT_NAME})
    
    # Install test plugin
    install(TARGETS ${COMPONENT_NAME}
        RUNTIME DESTINATION bin/plugins
        LIBRARY DESTINATION lib/daqster/plugins
        ARCHIVE DESTINATION lib
    )
    
    # Set RPATH for test plugins
    set_target_properties(${COMPONENT_NAME} PROPERTIES
        INSTALL_RPATH "$ORIGIN/../../lib"
    )
    
    message(STATUS "Created test component: ${COMPONENT_NAME}")
endfunction()
