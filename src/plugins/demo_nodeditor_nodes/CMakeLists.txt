cmake_minimum_required(VERSION 3.16)

project(demo_nodeditor_nodes VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# ── Plugin infrastructure ────────────────────────────────────────────
set(PLUGIN_SOURCES
    plugin/DemoNodeEditorNodesInterface.cpp
    plugin/DemoNodeEditorNodesInterface.h
    plugin/DemoNodeEditorNodesObject.cpp
    plugin/DemoNodeEditorNodesObject.h
)

# ── Sources category ────────────────────────────────────────────────
set(SOURCES_NODES
    Sources/NumberSourceDataModel.cpp
    Sources/NumberSourceDataModel.h
    Sources/NumberSourceDataUi.cpp
    Sources/NumberSourceDataUi.h
    Sources/NumberSourceDataUi.ui
)

# ── Displays category ───────────────────────────────────────────────
set(DISPLAY_NODES
    Displays/NumberDisplayDataModel.cpp
    Displays/NumberDisplayDataModel.h
    Displays/AudioDisplayModel.cpp
    Displays/AudioDisplayModel.h
    Displays/GenericDisplayNode.cpp
    Displays/GenericDisplayNode.h
)

# ── Operators category ──────────────────────────────────────────────
set(OPERATOR_NODES
    Operators/ModuloModel.cpp
    Operators/ModuloModel.h
)

# ── Routing category ────────────────────────────────────────────────
set(ROUTING_NODES
    Routing/DemuxNode.cpp
    Routing/DemuxNode.h
    Routing/MuxNode.cpp
    Routing/MuxNode.h
)

# ── Shared types ────────────────────────────────────────────────────
set(SHARED_TYPES
    shared/IntegerData.h
    shared/NumericType.h
)

# ── Use the Daqster plugin template ──────────────────────────────────
create_plugin(DemoNodeEditorNodes
    SOURCES
        ${PLUGIN_SOURCES}
        ${SOURCES_NODES}
        ${DISPLAY_NODES}
        ${OPERATOR_NODES}
        ${ROUTING_NODES}
        ${SHARED_TYPES}
    REQUIRES_LIBRARIES
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        Qt${QT_VERSION_MAJOR}::Charts
        Qt${QT_VERSION_MAJOR}::Multimedia
        QtNodes
        frame_work
    INCLUDE_DIRECTORIES
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/shared
        ${DAQSTER_SOURCE_DIR}/src/plugins/capabilities
        ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor_ide/BuiltInNodes/Library/types
        ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor_ide/BuiltInNodes/Library/connectors
        ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor_ide/BuiltInNodes/Library/display
        ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor_ide/BuiltInNodes/Library/decoders
)

# ── Output name must contain "plugin" substring ──────────────────────
if(TARGET DemoNodeEditorNodes)
    set_target_properties(DemoNodeEditorNodes PROPERTIES
        OUTPUT_NAME "DemoNodeEditorNodesPlugin"
    )
endif()
