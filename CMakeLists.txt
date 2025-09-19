cmake_minimum_required(VERSION 3.16)

project(Daqster VERSION 0.2.0 LANGUAGES CXX)

# Add cmake modules to path
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Detect Qt version (Qt6 preferred, Qt5 fallback)
include(FindQtVersion)

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

add_subdirectory(src/apps)
add_subdirectory(src/frame_work)

# External libraries - conditional based on Qt version and dependencies
message(STATUS "=== External Libraries Status ===")

if(QT_VERSION_MAJOR EQUAL 5)
    # Check Qt5 dependencies for external libraries
    if(QT_MULTIMEDIA_LIB AND NOT "${QT_MULTIMEDIA_LIB}" STREQUAL "")
        add_subdirectory(src/external_libs/nodeeditor)
        message(STATUS "NodeEditor library enabled for Qt5 (Multimedia available)")
    else()
        message(STATUS "NodeEditor library disabled for Qt5 (Multimedia not available)")
    endif()
    
    if(QT_QUICKCONTROLS2_LIB AND NOT "${QT_QUICKCONTROLS2_LIB}" STREQUAL "")
        add_subdirectory(src/external_libs/qtrest_lib)
        message(STATUS "QtRest library enabled for Qt5 (QuickControls2 available)")
    else()
        message(STATUS "QtRest library disabled for Qt5 (QuickControls2 not available)")
    endif()
else()
    message(STATUS "NodeEditor and QtRest libraries disabled for Qt6 (compatibility issues)")
endif()

# Plugins - after external libraries are processed
add_subdirectory(src/plugins)

