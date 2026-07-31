# Unit tests for the Requirements Manager plugin logic.
#
# The plugin logic under test (RequirementsParser/RequirementsModel/
# RequirementsValidator) is compiled directly into the test binary instead of
# linking the plugin .so - the model classes are QtCore-only, so the tests run
# headless (no QApplication, no GUI).
#
# The three test classes (TestParser/TestModel/TestValidator) are declared in
# headers and share a single binary whose main() lives in test_main.cpp. Each
# class is run through QTest::qExec there, mirroring QTEST_GUILESS_MAIN.

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
    test_parser.h
    test_model.h
    test_validator.h
    # plugin logic under test (compiled directly into the test binary):
    ${DAQSTER_SOURCE_DIR}/src/plugins/requirements_manager/RequirementsParser.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/requirements_manager/RequirementsModel.cpp
    ${DAQSTER_SOURCE_DIR}/src/plugins/requirements_manager/RequirementsValidator.cpp
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
