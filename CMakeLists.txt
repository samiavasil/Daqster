cmake_minimum_required(VERSION 3.16)

# Single source of truth: root VERSION file (X.Y.Z)
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION")
    message(FATAL_ERROR "VERSION file not found at ${CMAKE_CURRENT_SOURCE_DIR}/VERSION")
endif()
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" DAQSTER_VERSION LIMIT_COUNT 1)
string(STRIP "${DAQSTER_VERSION}" DAQSTER_VERSION)
if(NOT DAQSTER_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
    message(FATAL_ERROR "VERSION must be X.Y.Z (got '${DAQSTER_VERSION}')")
endif()
string(REPLACE "." ";" _daqster_version_parts "${DAQSTER_VERSION}")
list(GET _daqster_version_parts 0 DAQSTER_VERSION_MAJOR)
list(GET _daqster_version_parts 1 DAQSTER_VERSION_MINOR)
list(GET _daqster_version_parts 2 DAQSTER_VERSION_PATCH)
project(Daqster VERSION ${DAQSTER_VERSION} LANGUAGES CXX)

# Generate daqster_version.h from the VERSION file (single source of truth)
set(DAQSTER_VERSION_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/include")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/daqster_version.h.in"
    "${DAQSTER_VERSION_INCLUDE_DIR}/daqster_version.h"
    @ONLY
)

set(DAQSTER_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

include(GNUInstallDirs)

# Add cmake modules to path
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Control verbose dependency checks (default: minimal output)
option(DAQSTER_VERBOSE_DEPENDENCIES "Show detailed dependency checks" OFF)

# Automatically initialize missing external submodule directories when enabled
option(DAQSTER_AUTO_INIT_SUBMODULES "Automatically run 'git submodule update --init' for missing external submodules" ON)

# Detect Qt version (Qt6 preferred, Qt5 fallback)
include(FindQtVersion)

# Propagate QT_VERSION_MAJOR to parent scope when included via add_subdirectory()
if(NOT CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(QT_VERSION_MAJOR ${QT_VERSION_MAJOR} PARENT_SCOPE)
endif()

# Propagate version to parent scope when included via add_subdirectory()
if(NOT CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(DAQSTER_VERSION ${DAQSTER_VERSION} PARENT_SCOPE)
    set(DAQSTER_VERSION_MAJOR ${DAQSTER_VERSION_MAJOR} PARENT_SCOPE)
    set(DAQSTER_VERSION_MINOR ${DAQSTER_VERSION_MINOR} PARENT_SCOPE)
    set(DAQSTER_VERSION_PATCH ${DAQSTER_VERSION_PATCH} PARENT_SCOPE)
    set(DAQSTER_VERSION_INCLUDE_DIR ${DAQSTER_VERSION_INCLUDE_DIR} PARENT_SCOPE)
endif()

# Propagate Qt version choice to external libraries (e.g. nodeeditor)
if(QT_VERSION_MAJOR EQUAL 6)
    set(USE_QT6 ON CACHE BOOL "Build external libs with Qt6" FORCE)
else()
    set(USE_QT6 OFF CACHE BOOL "Build external libs with Qt6" FORCE)
endif()

# Include plugin dependency manager
include(PluginDependencyManager)

# Include component templates
include(ComponentTemplates)

# Try to find git for optional submodule initialization
find_package(Git QUIET)

# Use CMAKE_BINARY_DIR (not PROJECT_BINARY_DIR) so outputs land in the consuming
# project's bin/ when Daqster is included via add_subdirectory(). When building
# standalone, CMAKE_BINARY_DIR == PROJECT_BINARY_DIR so behavior is unchanged.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY  ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY  ${CMAKE_BINARY_DIR}/bin)

#set_target_properties(Daqster PROPERTIES LIBRARY_OUTPUT_DIRECTORY $<1:${PROJECT_SOURCE_DIR}/lib>)
#set_target_properties(Daqster PROPERTIES RUNTIME_OUTPUT_DIRECTORY $<1:${PROJECT_SOURCE_DIR}/bin>)

set(CMAKE_CXX_STANDARD 17)
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
set(CMAKE_BUILD_RPATH "${PROJECT_BINARY_DIR}/bin")

add_subdirectory(src/frame_work)

# External libraries - conditional based on Qt version and dependencies
message(STATUS "=== External Libraries Status ===")

# Find required packages for external libraries
find_package(OpenSSL)

# Find only the core Qt modules needed for basic functionality
# Other modules will be found dynamically by PluginDependencyManager
find_package(Qt${QT_VERSION_MAJOR} QUIET COMPONENTS Core Gui Widgets)

# NodeEditor - Qt5/Qt6 (upstream has unified Qt${QT_VERSION_MAJOR} handling)
create_external_library(nodeeditor)

# QtRest library - Qt5/Qt6 dual-mode
create_external_library(qtrest_lib)

# Plugins - dependency система автоматично изключва неподдържани
add_subdirectory(src/plugins/node_editor_ide)
add_subdirectory(src/plugins/demo_nodeditor_nodes)
add_subdirectory(src/plugins/requirements_manager)
add_subdirectory(src/plugins/QtCoinTrader)

# Test plugins - gated behind their own option (default OFF) so they are not
# part of normal packaging/release builds. Independent from DAQSTER_BUILD_TESTS:
# unit tests still build with DAQSTER_BUILD_TESTS=ON; only the test PLUGINS are
# gated here.
option(DAQSTER_BUILD_TEST_PLUGINS "Build test plugins" OFF)
if(DAQSTER_BUILD_TEST_PLUGINS)
    add_subdirectory(src/plugins/tests/plugin_main_test)
    add_subdirectory(src/plugins/tests/plugin_fancy_test)
    add_subdirectory(src/plugins/tests/plugin_uggly_test)
    add_subdirectory(src/plugins/tests/template_plugin_daqster)
endif()

# Apps - after plugins are processed (frame_work is built in plugins)
add_subdirectory(src/apps/Daqster)

# Unit tests - gated behind their own option (default OFF).
# We intentionally do NOT set BUILD_TESTING / include(CTest) here: nodeeditor
# defines its own BUILD_TESTING option and enabling it would pull in its
# Catch2 dependency download.
option(DAQSTER_BUILD_TESTS "Build unit tests" OFF)
if(DAQSTER_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Print final build configuration summary
print_build_configuration_summary()

