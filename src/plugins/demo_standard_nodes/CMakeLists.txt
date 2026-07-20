cmake_minimum_required(VERSION 3.16)

project(demo_standard_nodes VERSION 0.1.0 LANGUAGES CXX)

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
create_plugin(DemoStandardNodes
    SOURCES
        ${NODE_SOURCES}
        DemoStandardNodesInterface.cpp
        DemoStandardNodesInterface.h
        DemoStandardNodesObject.cpp
        DemoStandardNodesObject.h
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
if(TARGET DemoStandardNodes)
    set_target_properties(DemoStandardNodes PROPERTIES
        OUTPUT_NAME "DemoStandardNodesPlugin"
    )
endif()
