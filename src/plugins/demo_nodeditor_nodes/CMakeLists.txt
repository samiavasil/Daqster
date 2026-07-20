cmake_minimum_required(VERSION 3.16)

project(demo_nodeditor_nodes VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# ── Node model sources ───────────────────────────────────────────────
set(NODE_SOURCES
    NumberSourceDataModel.cpp
    NumberSourceDataModel.h
    NumberDisplayDataModel.cpp
    NumberDisplayDataModel.h
    ModuloModel.cpp
    ModuloModel.h
    NumberSourceDataUi.cpp
    NumberSourceDataUi.h
    NumberSourceDataUi.ui
    NumericType.h
    IntegerData.h
)

# ── Use the Daqster plugin template ──────────────────────────────────
create_plugin(DemoNodeEditorNodes
    SOURCES
        ${NODE_SOURCES}
        DemoNodeEditorNodesInterface.cpp
        DemoNodeEditorNodesInterface.h
        DemoNodeEditorNodesObject.cpp
        DemoNodeEditorNodesObject.h
    REQUIRES_LIBRARIES
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        QtNodes
        frame_work
    INCLUDE_DIRECTORIES
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${DAQSTER_SOURCE_DIR}/src/plugins/capabilities
)

# ── Output name must contain "plugin" substring ──────────────────────
if(TARGET DemoNodeEditorNodes)
    set_target_properties(DemoNodeEditorNodes PROPERTIES
        OUTPUT_NAME "DemoNodeEditorNodesPlugin"
    )
endif()
