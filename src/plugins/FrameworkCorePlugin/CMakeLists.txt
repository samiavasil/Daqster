cmake_minimum_required(VERSION 3.16)

project(FrameworkCorePlugin VERSION 0.3.2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# ── Plugin JSON metadata (generated from template) ──────────────────
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/FrameworkCorePluginInterface.json.in"
    "${CMAKE_CURRENT_SOURCE_DIR}/FrameworkCorePluginInterface.json"
    @ONLY
)

# ── Plugin infrastructure (root level) ──────────────────────────────
set(PLUGIN_SOURCES
    FrameworkCorePluginInterface.cpp
    FrameworkCorePluginInterface.h
    FrameworkCorePluginObject.cpp
    FrameworkCorePluginObject.h
)

# ── Use the Daqster plugin template ──────────────────────────────────
create_plugin(FrameworkCorePlugin
    SOURCES
        ${PLUGIN_SOURCES}
    REQUIRES_LIBRARIES
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        Qt${QT_VERSION_MAJOR}::Multimedia
        Qt${QT_VERSION_MAJOR}::Network
        Qt${QT_VERSION_MAJOR}::OpenGL
        QtNodes
        FrameworkCore
        DemoNodeEditorNodesCore
    INCLUDE_DIRECTORIES
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${DAQSTER_SOURCE_DIR}/src/plugins/common
        ${DAQSTER_SOURCE_DIR}/src/frame_work/base/src/include
        ${DAQSTER_SOURCE_DIR}/src/frame_work/base/src/logging
        ${DAQSTER_SOURCE_DIR}/src/frame_work/base/src/engine
        ${DAQSTER_SOURCE_DIR}/src/plugins/demo_nodeditor_nodes_core
        ${DAQSTER_SOURCE_DIR}/src/plugins/demo_nodeditor_nodes_core/Sources
        ${DAQSTER_SOURCE_DIR}/src/plugins/demo_nodeditor_nodes_core/Displays
        ${DAQSTER_SOURCE_DIR}/src/plugins/demo_nodeditor_nodes_core/Operators
        ${DAQSTER_SOURCE_DIR}/src/plugins/demo_nodeditor_nodes_core/shared
)

# ── Output name must contain "plugin" substring ──────────────────────
if(TARGET FrameworkCorePlugin)
    set_target_properties(FrameworkCorePlugin PROPERTIES
        OUTPUT_NAME "FrameworkCorePlugin"
    )
endif()