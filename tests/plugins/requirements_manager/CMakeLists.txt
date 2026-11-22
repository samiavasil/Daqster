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
    test_sorting.cpp
    test_report.cpp
    test_dates.cpp
    test_dates.h
    test_parser.h
    test_model.h
    test_validator.h
    test_graph.h
    test_graph_layout.h
    test_search.h
    test_merge.h
    test_sorting.h
    test_report.h
    # plugin logic under test (compiled directly into the test binary):
    ${DAQSTER_SOURCE_DIR}/src/plugins/requirements_manager/RequirementsParser.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/requirements_manager/DoneGate.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/requirements_manager/RequirementsModel.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/requirements_manager/RequirementsSortProxyModel.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/requirements_manager/RequirementsValidator.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/requirements_manager/ValidationReport.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/requirements_manager/TraceabilityMatrixModel.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/requirements_manager/RequirementsSearchEngine.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/requirements_manager/RequirementDates.cpp
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
    # RequirementsModel returns the overdue-DRAFT colour through
    # Qt::ForegroundRole (REQ-SW-PL-100), so QColor's symbols are needed even
    # though the binary itself is headless.
    Qt${QT_VERSION_MAJOR}::Gui
    FrameworkCore
    NodeEditorLibrary
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
    FrameworkCore
    NodeEditorLibrary
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
    FrameworkCore
    NodeEditorLibrary
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
    ${DAQSTER_SOURCE_DIR}/src/plugins/requirements_manager/RequirementDates.cpp
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
    FrameworkCore
    NodeEditorLibrary
)

add_test(NAME requirements_manager_gui_tests
         COMMAND $<TARGET_FILE:requirements_manager_gui_tests>)
set_tests_properties(requirements_manager_gui_tests PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# GUI tests for the Requirements Manager widgets themselves (REQ-SW-PL-001):
# compact action toolbar (minimum width), full-Markdown preview, validation
# copy actions, traceability metrics panel + sorting. The widget sources are
# compiled directly into the binary (they carry no plugin registration), and
# QTEST_MAIN in test_widgets.cpp provides a QApplication; the offscreen
# platform plugin keeps the run headless.
set(WIDGET_TEST_SOURCES
    test_widgets.cpp
    # plugin logic under test (compiled directly into the test binary):
    ${DAQSTER_SOURCE_DIR}/src/plugins/requirements_manager/RequirementsWidget.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/requirements_manager/RequirementsWidget.h
    ${DAQSTER_SOURCE_DIR}/src/plugins/requirements_manager/RequirementPreview.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/requirements_manager/ValidationDialog.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/requirements_manager/ValidationDialog.h
    ${DAQSTER_SOURCE_DIR}/src/plugins/requirements_manager/HelpDialog.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/requirements_manager/HelpDialog.h
    ${DAQSTER_SOURCE_DIR}/src/plugins/requirements_manager/DraftNoteDialog.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/requirements_manager/DraftNoteDialog.h
    ${DAQSTER_SOURCE_DIR}/src/plugins/requirements_manager/NewRequirementDialog.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/requirements_manager/NewRequirementDialog.h
    ${DAQSTER_SOURCE_DIR}/src/plugins/requirements_manager/TraceabilityMatrixWidget.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/requirements_manager/TraceabilityMatrixWidget.h
    ${DAQSTER_SOURCE_DIR}/src/plugins/requirements_manager/TraceabilityMatrixModel.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/requirements_manager/TraceabilityMatrixModel.h
    ${DAQSTER_SOURCE_DIR}/src/plugins/requirements_manager/DependencyGraphWidget.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/requirements_manager/DependencyGraphWidget.h
    ${DAQSTER_SOURCE_DIR}/src/plugins/requirements_manager/DependencyGraphData.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/requirements_manager/DependencyGraphLayout.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/requirements_manager/MatrixExporter.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/requirements_manager/RequirementsModel.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/requirements_manager/RequirementsModel.h
    ${DAQSTER_SOURCE_DIR}/src/plugins/requirements_manager/RequirementsSortProxyModel.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/requirements_manager/RequirementsSearchEngine.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/requirements_manager/IdeaReader.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/requirements_manager/IdeaReader.h
    ${DAQSTER_SOURCE_DIR}/src/plugins/requirements_manager/IdeaModel.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/requirements_manager/IdeaModel.h
    ${DAQSTER_SOURCE_DIR}/src/plugins/requirements_manager/RequirementsParser.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/requirements_manager/DoneGate.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/requirements_manager/RequirementsValidator.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/requirements_manager/RequirementDates.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/requirements_manager/RequirementDates.h
    ${DAQSTER_SOURCE_DIR}/src/plugins/requirements_manager/ValidationReport.cpp
)

add_executable(requirements_manager_widget_tests ${WIDGET_TEST_SOURCES})

target_include_directories(requirements_manager_widget_tests PRIVATE
    ${DAQSTER_SOURCE_DIR}/src/plugins/requirements_manager
    ${DAQSTER_SOURCE_DIR}/src/plugins/common
    ${DAQSTER_SOURCE_DIR}/src/frame_work/base/src/include   # for LogCategories.h
)

target_link_libraries(requirements_manager_widget_tests PRIVATE
    Qt${QT_VERSION_MAJOR}::Test
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Widgets
    FrameworkCore
    NodeEditorLibrary
)

add_test(NAME requirements_manager_widget_tests
         COMMAND $<TARGET_FILE:requirements_manager_widget_tests>)
set_tests_properties(requirements_manager_widget_tests PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# GUI tests for the read-only "Идеи" panel (REQ-SW-PL-095). The source set is
# the widget-test set with a different test main: the panel reuses
# RequirementsWidget and RequirementPreview, so the same plugin sources are
# compiled in. Separate binary on purpose — these tests also prove the panel
# leaves the validator/traceability pipeline untouched.
set(IDEAS_TEST_SOURCES ${WIDGET_TEST_SOURCES})
list(REMOVE_ITEM IDEAS_TEST_SOURCES test_widgets.cpp)
list(APPEND IDEAS_TEST_SOURCES test_ideas.cpp)

add_executable(requirements_manager_ideas_tests ${IDEAS_TEST_SOURCES})

target_include_directories(requirements_manager_ideas_tests PRIVATE
    ${DAQSTER_SOURCE_DIR}/src/plugins/requirements_manager
    ${DAQSTER_SOURCE_DIR}/src/plugins/common
    ${DAQSTER_SOURCE_DIR}/src/frame_work/base/src/include   # for LogCategories.h
)

target_link_libraries(requirements_manager_ideas_tests PRIVATE
    Qt${QT_VERSION_MAJOR}::Test
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Widgets
    FrameworkCore
    NodeEditorLibrary
)

add_test(NAME requirements_manager_ideas_tests
         COMMAND $<TARGET_FILE:requirements_manager_ideas_tests>)
set_tests_properties(requirements_manager_ideas_tests PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
