cmake_minimum_required(VERSION 3.16)

project(requirements_manager VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

set(PLUGIN_SOURCES
    RequirementsManagerInterface.cpp
    RequirementsManagerInterface.h
    RequirementsManagerObject.cpp
    RequirementsManagerObject.h
    RequirementsParser.cpp
    RequirementsParser.h
    RequirementsModel.cpp
    RequirementsModel.h
    RequirementsWidget.cpp
    RequirementsWidget.h
    RequirementsValidator.cpp
    RequirementsValidator.h
    RequirementsSearchEngine.cpp
    RequirementsSearchEngine.h
    DependencyGraphData.cpp
    DependencyGraphData.h
    DependencyGraphLayout.cpp
    DependencyGraphLayout.h
    DependencyGraphWidget.cpp
    DependencyGraphWidget.h
    NewRequirementDialog.cpp
    NewRequirementDialog.h
    ValidationDialog.cpp
    ValidationDialog.h
    HelpDialog.cpp
    HelpDialog.h
    MatrixExporter.cpp
    MatrixExporter.h
    TraceabilityMatrixModel.cpp
    TraceabilityMatrixModel.h
    TraceabilityMatrixWidget.cpp
    TraceabilityMatrixWidget.h
)

create_plugin(RequirementsManager
    SOURCES
        ${PLUGIN_SOURCES}
    REQUIRES_LIBRARIES
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        Qt${QT_VERSION_MAJOR}::Widgets
        frame_work
    INCLUDE_DIRECTORIES
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${DAQSTER_SOURCE_DIR}/src/plugins/common
)

if(TARGET RequirementsManager)
    set_target_properties(RequirementsManager PROPERTIES
        OUTPUT_NAME "RequirementsManagerPlugin"
    )
endif()
