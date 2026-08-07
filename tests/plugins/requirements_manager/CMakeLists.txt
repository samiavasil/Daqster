# Unit tests for the Requirements Manager plugin logic.
#
# The plugin logic under test (RequirementsParser/RequirementsModel/
# RequirementsValidator/DependencyGraphData/TraceabilityMatrixModel/
# MatrixExporter) is compiled directly into the test binaries instead of
# linking the plugin .so - the model classes are QtCore-only, so the tests run
# headless (no QApplication, no GUI).
#
# The headless test classes (TestParser/TestModel/TestValidator/TestGraph) are
# declared in headers and share a single binary (requirements_manager_tests)
# whose main() lives in test_main.cpp. Each class is run through QTest::qExec
# there, mirroring QTEST_GUILESS_MAIN.
#
# The REQ-SW-PL-010 traceability tests are self-contained QTEST_MAIN executables
# (test_matrix.cpp / test_exporter.cpp define their own main), so each gets its
# own binary: requirements_manager_matrix_tests and
# requirements_manager_exporter_tests.
#
# The interactive graph (DependencyGraphWidget) is GUI-only and lives in its
# own binary (requirements_manager_gui_tests) that runs under QApplication
# (QTEST_MAIN) with the offscreen platform plugin; see the target below.

find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Test REQUIRED)

# RequirementsModel.h contains Q_OBJECT - the root CMakeLists.txt already sets
# CMAKE_AUTOMOC ON globally; we repeat it here so this directory is
# self-contained when tests are built standalone.
set(CMAKE_AUTOMOC ON)

set(TEST_SOURCES
    test_main.cpp
    test_parser.cpp
    test_model.cpp
    test_validator.cpp
    test_graph.cpp
    test_graph_layout.cpp
    test_search.cpp
    test_merge.cpp
    test_parser.h
    test_model.h
    test_validator.h
    test_graph.h
    test_graph_layout.h
    test_search.h
    test_merge.h
    # plugin logic under test (compiled directly into the test binary):
    ${DAQSTER_SOURCE_DIR}/src/plugins/requirements_manager/RequirementsParser.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/requirements_manager/RequirementsModel.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/requirements_manager/RequirementsValidator.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/requirements_manager/RequirementsSearchEngine.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/requirements_manager/DependencyGraphData.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/requirements_manager/DependencyGraphLayout.cpp
)

add_executable(requirements_manager_tests ${TEST_SOURCES})

target_include_directories(requirements_manager_tests PRIVATE
    ${DAQSTER_SOURCE_DIR}/src/plugins/requirements_manager
    ${DAQSTER_SOURCE_DIR}/src/plugins/common
    ${DAQSTER_SOURCE_DIR}/src/frame_work/base/src/include   # for LogCategories.h
)

target_link_libraries(requirements_manager_tests PRIVATE
    Qt${QT_VERSION_MAJOR}::Test
    Qt${QT_VERSION_MAJOR}::Core
    frame_work
)

add_test(NAME requirements_manager_tests COMMAND $<TARGET_FILE:requirements_manager_tests>)

# REQ-SW-PL-010 traceability tests. The traceability model/exporter classes are
# QtCore-only, and each test file is self-contained (QTEST_MAIN in the .cpp
# provides its own main), so each gets its own headless executable.
set(MATRIX_TEST_SOURCES
    test_matrix.cpp
    # plugin logic under test (compiled directly into the test binary):
    ${DAQSTER_SOURCE_DIR}/src/plugins/requirements_manager/TraceabilityMatrixModel.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/requirements_manager/MatrixExporter.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/requirements_manager/RequirementsParser.cpp
)

add_executable(requirements_manager_matrix_tests ${MATRIX_TEST_SOURCES})

target_include_directories(requirements_manager_matrix_tests PRIVATE
    ${DAQSTER_SOURCE_DIR}/src/plugins/requirements_manager
    ${DAQSTER_SOURCE_DIR}/src/plugins/common
    ${DAQSTER_SOURCE_DIR}/src/frame_work/base/src/include   # for LogCategories.h
)

target_link_libraries(requirements_manager_matrix_tests PRIVATE
    Qt${QT_VERSION_MAJOR}::Test
    Qt${QT_VERSION_MAJOR}::Core
    frame_work
)

add_test(NAME requirements_manager_matrix_tests COMMAND $<TARGET_FILE:requirements_manager_matrix_tests>)

set(EXPORTER_TEST_SOURCES
    test_exporter.cpp
    # plugin logic under test (compiled directly into the test binary):
    ${DAQSTER_SOURCE_DIR}/src/plugins/requirements_manager/MatrixExporter.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/requirements_manager/RequirementsParser.cpp
)

add_executable(requirements_manager_exporter_tests ${EXPORTER_TEST_SOURCES})

target_include_directories(requirements_manager_exporter_tests PRIVATE
    ${DAQSTER_SOURCE_DIR}/src/plugins/requirements_manager
    ${DAQSTER_SOURCE_DIR}/src/plugins/common
    ${DAQSTER_SOURCE_DIR}/src/frame_work/base/src/include   # for LogCategories.h
)

target_link_libraries(requirements_manager_exporter_tests PRIVATE
    Qt${QT_VERSION_MAJOR}::Test
    Qt${QT_VERSION_MAJOR}::Core
    frame_work
)

add_test(NAME requirements_manager_exporter_tests COMMAND $<TARGET_FILE:requirements_manager_exporter_tests>)

# GUI tests for the interactive dependency graph (Bug A: edges follow nodes on
# drag; Bug B: viewport fit on resize/show). DependencyGraphWidget and its
# dependency closure are compiled directly into the binary. QTEST_MAIN in
# test_graph_widget.cpp provides a QApplication main; the offscreen platform
# plugin keeps the tests headless on CI/dev boxes without a display.
set(GUI_TEST_SOURCES
    test_graph_widget.cpp
    test_graph_widget.h
    # plugin logic under test (compiled directly into the test binary):
    ${DAQSTER_SOURCE_DIR}/src/plugins/requirements_manager/DependencyGraphWidget.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/requirements_manager/DependencyGraphData.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/requirements_manager/DependencyGraphLayout.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/requirements_manager/RequirementsParser.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/requirements_manager/RequirementsValidator.cpp
)

add_executable(requirements_manager_gui_tests ${GUI_TEST_SOURCES})

target_include_directories(requirements_manager_gui_tests PRIVATE
    ${DAQSTER_SOURCE_DIR}/src/plugins/requirements_manager
    ${DAQSTER_SOURCE_DIR}/src/plugins/common
    ${DAQSTER_SOURCE_DIR}/src/frame_work/base/src/include   # for LogCategories.h
)

target_link_libraries(requirements_manager_gui_tests PRIVATE
    Qt${QT_VERSION_MAJOR}::Test
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Widgets
    frame_work
)

add_test(NAME requirements_manager_gui_tests
         COMMAND $<TARGET_FILE:requirements_manager_gui_tests>)
set_tests_properties(requirements_manager_gui_tests PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
