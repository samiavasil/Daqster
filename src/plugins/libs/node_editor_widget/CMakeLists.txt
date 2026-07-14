cmake_minimum_required(VERSION 3.16)

project(node_editor_widget VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# ── Sources ──────────────────────────────────────────────────────────
set(WIDGET_SOURCES
    src/NodeEditorWidget.cpp
    src/NodeEditorWidget.h
    src/NodeEditorWidget_global.h
    src/ChatGraphModel.h
)

# ── Use internal library template (participates in dependency system) ─
create_internal_library(node_editor_widget
    SOURCES
        ${WIDGET_SOURCES}
    REQUIRES_LIBRARIES
        QtNodes
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Widgets
)

target_compile_definitions(node_editor_widget PRIVATE NODE_EDITOR_WIDGET_LIBRARY)

target_include_directories(node_editor_widget
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# ── Output to shared plugins directory ───────────────────────────────
set_target_properties(node_editor_widget PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/plugins
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/plugins
)

# ── Install headers ─────────────────────────────────────────────────
install(FILES
    src/NodeEditorWidget.h
    src/NodeEditorWidget_global.h
    DESTINATION include
)
