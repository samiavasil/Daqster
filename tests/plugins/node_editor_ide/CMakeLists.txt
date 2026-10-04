# Unit tests for the node_editor_ide workspace layout engine (REQ-SW-PL-053
# Phase 2a): LayoutTree (QtCore-only schema-v2 data model + JSON) and
# LayoutEngine (QtWidgets builder/capturer).
#
# Both sources are compiled directly into the test binary (same pattern as
# demo_nodeditor_nodes_tests) instead of linking the plugin .so. The test
# classes are declared in headers and share a single binary
# (node_editor_ide_tests) whose main() lives in test_main.cpp — QApplication,
# because LayoutEngine builds real widgets. The binary runs headless via the
# offscreen platform plugin.

find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Test Widgets REQUIRED)

# The test classes declare Q_OBJECT in their headers - the root CMakeLists.txt
# already sets CMAKE_AUTOMOC ON globally; we repeat it here so this directory
# is self-contained when tests are built standalone.
set(CMAKE_AUTOMOC ON)

set(TEST_SOURCES
    test_main.cpp
    test_layout_tree.cpp test_layout_tree.h
    test_layout_engine.cpp test_layout_engine.h
    # plugin logic under test (compiled directly into the test binary):
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor_ide/LayoutTree.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor_ide/LayoutEngine.cpp
    # LayoutTree reuses FlowUi::Geometry/NodeUi from the v1 section model:
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor_ide/FlowUiSection.cpp
)

add_executable(node_editor_ide_tests ${TEST_SOURCES})

target_include_directories(node_editor_ide_tests PRIVATE
    ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor_ide
    ${DAQSTER_SOURCE_DIR}/src/plugins/external_libs/nodeeditor/include
)

target_link_libraries(node_editor_ide_tests PRIVATE
    Qt${QT_VERSION_MAJOR}::Test
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Widgets
    # LayoutTree.h/LayoutEngine.h include QtNodes/Definitions (NodeId typedef);
    # the QtNodes target also propagates the NODE_EDITOR_SHARED definition.
    QtNodes
)

add_test(NAME node_editor_ide_tests
         COMMAND node_editor_ide_tests)
set_tests_properties(node_editor_ide_tests PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")