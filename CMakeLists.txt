cmake_minimum_required(VERSION 3.16)

project(Daqster VERSION 0.2.0 LANGUAGES CXX)

# Add cmake modules to path
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Detect Qt version (Qt6 preferred, Qt5 fallback)
include(FindQtVersion)

# Include plugin dependency manager
include(PluginDependencyManager)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY  ${PROJECT_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY  ${PROJECT_BINARY_DIR}/bin)

#set_target_properties(Daqster PROPERTIES LIBRARY_OUTPUT_DIRECTORY $<1:${PROJECT_SOURCE_DIR}/lib>)
#set_target_properties(Daqster PROPERTIES RUNTIME_OUTPUT_DIRECTORY $<1:${PROJECT_SOURCE_DIR}/bin>)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(CMAKE_VERSION VERSION_LESS "3.7.0")
    set(CMAKE_INCLUDE_CURRENT_DIR ON)
endif()

# RPATH settings for finding installed shared libs at runtime
set(CMAKE_SKIP_BUILD_RPATH FALSE)
set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)
set(CMAKE_INSTALL_RPATH "$ORIGIN/../lib")
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
set(CMAKE_BUILD_RPATH "${CMAKE_BINARY_DIR}/bin")

add_subdirectory(src/frame_work)

# External libraries - conditional based on Qt version and dependencies
message(STATUS "=== External Libraries Status ===")

# Find required packages for external libraries
find_package(OpenSSL)

# Find only the core Qt modules needed for basic functionality
# Other modules will be found dynamically by PluginDependencyManager
find_package(Qt${QT_VERSION_MAJOR} QUIET COMPONENTS Core Gui Widgets)

# Add external libraries directly - they check their own dependencies
if(QT_VERSION_MAJOR EQUAL 5)
    # NodeEditor library - will check its own dependencies in its CMakeLists.txt
    add_subdirectory(src/external_libs/nodeeditor)
    message(STATUS "nodeeditor external library: ENABLED")
    
    # QtRest library - will check its own dependencies in its CMakeLists.txt
    add_subdirectory(src/external_libs/qtrest_lib)
    message(STATUS "qtrest_lib external library: ENABLED")
    
    # Register external libraries as available dependencies
    register_external_library_dependency(nodes)
    register_external_library_dependency(qtrest_lib)
    
else()
    message(STATUS "External libraries disabled for Qt6 (compatibility issues)")
    message(STATUS "  - NodeEditor: Qt6 compatibility issues")
    message(STATUS "  - QtRest: Qt6 compatibility issues")
endif()

# Plugins - after external libraries are processed
add_subdirectory(src/plugins)

# Apps - after plugins are processed (frame_work is built in plugins)
add_subdirectory(src/apps)

# Print final build configuration summary
print_build_configuration_summary()

