# Scene close contract tests (REQ-SW-PL-078).
#
# The close half of the scene lifecycle: stop every node (sources first), tell
# the plugins (ISceneLifecycle::onSceneClosed()), and only then let the graph
# destroy the models. FrameworkCore::SceneClose is the single implementation all
# three hosts (IDE, GUI runtime, headless engine) call, so it is tested here once
# with fake node models — no plugin, no widgets, no models on disk.

find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Test Core Gui REQUIRED)

set(CMAKE_AUTOMOC ON)

add_executable(scene_close_tests
    test_scene_close.cpp
)

target_include_directories(scene_close_tests PRIVATE
    ${DAQSTER_SOURCE_DIR}/src/plugins/common
)

target_link_libraries(scene_close_tests PRIVATE
    FrameworkCore
    Qt${QT_VERSION_MAJOR}::Test
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    QtNodes
)

add_test(NAME scene_close_tests COMMAND $<TARGET_FILE:scene_close_tests>)

# Scene open hook (REQ-SW-PL-082).
#
# The other half of the scene lifecycle: ask the plugins BEFORE load(), so a
# refusal leaves the running scene untouched. Same shape as the close tests —
# FrameworkCore::SceneOpen with fake providers, no plugin and no models.
add_executable(scene_open_tests
    test_scene_open.cpp
)

target_include_directories(scene_open_tests PRIVATE
    ${DAQSTER_SOURCE_DIR}/src/plugins/common
)

target_link_libraries(scene_open_tests PRIVATE
    FrameworkCore
    Qt${QT_VERSION_MAJOR}::Test
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    QtNodes
)

add_test(NAME scene_open_tests COMMAND $<TARGET_FILE:scene_open_tests>)
