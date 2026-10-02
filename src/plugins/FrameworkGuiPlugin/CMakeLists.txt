cmake_minimum_required(VERSION 3.16)

project(FrameworkGuiPlugin VERSION 0.3.2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Windows: export all symbols so the plugin DLL can be loaded by QPluginLoader
if(WIN32)
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()

# ── Plugin JSON metadata (generated from template) ──────────────────
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/FrameworkGuiPluginInterface.json.in"
    "${CMAKE_CURRENT_SOURCE_DIR}/FrameworkGuiPluginInterface.json"
    @ONLY
)

# ── Plugin infrastructure (root level) ──────────────────────────────
set(PLUGIN_SOURCES
    FrameworkGuiPluginInterface.cpp
    FrameworkGuiPluginInterface.h
    FrameworkGuiPluginObject.cpp
    FrameworkGuiPluginObject.h
)

# ── GUI runtime (REQ-SW-PL-053: moved out of node_editor_ide) ──────
# RuntimeShell is the only runtime-specific file. The editor canvas pieces it
# builds on (FlowUiSection, NodeEditorWidget, CustomDataFlowScene) stay in
# node_editor_ide and are included from there.
set(RUNTIME_SOURCES
    RuntimeShell.cpp
    RuntimeShell.h
)

# ── Use the Daqster plugin template ──────────────────────────────────
create_plugin(FrameworkGuiPlugin
    SOURCES
        ${PLUGIN_SOURCES}
        ${RUNTIME_SOURCES}
    REQUIRES_LIBRARIES
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Multimedia
        Qt${QT_VERSION_MAJOR}::MultimediaWidgets
        Qt${QT_VERSION_MAJOR}::OpenGL
        Qt${QT_VERSION_MAJOR}::Charts
        Qt${QT_VERSION_MAJOR}::Network
        QtNodes
        FrameworkCore
        FrameworkGui
        DemoNodeEditorNodesCore
        DemoNodeEditorNodesGui
        NodeEditorLibrary
    INCLUDE_DIRECTORIES
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${DAQSTER_SOURCE_DIR}/src/plugins/common
        ${DAQSTER_SOURCE_DIR}/src/frame_work/base/src/include
        ${DAQSTER_SOURCE_DIR}/src/frame_work/base/src/logging
        ${DAQSTER_SOURCE_DIR}/src/frame_work/base/src/gui
        ${DAQSTER_SOURCE_DIR}/src/plugins/demo_nodeditor_nodes_core
        ${DAQSTER_SOURCE_DIR}/src/plugins/demo_nodeditor_nodes_core/Sources/Video
        ${DAQSTER_SOURCE_DIR}/src/plugins/demo_nodeditor_nodes_gui
        # Editor canvas pieces reused by RuntimeShell (kept in node_editor_ide)
        ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor_ide
        ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor_ide/BuiltInNodes
        ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor_ide/BuiltInNodes/Library
)

# Qt6 splits OpenGL widgets out of Qt::OpenGLWidgets; Qt5 has no such module.
if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    target_link_libraries(FrameworkGuiPlugin PUBLIC Qt${QT_VERSION_MAJOR}::OpenGLWidgets)
endif()

# Windows: export all symbols from the plugin DLL
if(WIN32)
    set_target_properties(FrameworkGuiPlugin PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()

# ── Output name must contain "plugin" substring (QPluginManager filter) ────
if(TARGET FrameworkGuiPlugin)
    set_target_properties(FrameworkGuiPlugin PROPERTIES
        OUTPUT_NAME "FrameworkGuiPlugin"
    )
endif()
