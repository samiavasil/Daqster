# IStatusBarProvider discovery (REQ-SW-PL-079).
#
# The capability is probed with dynamic_cast through PluginRegistry (it is a
# plain, non-QObject interface with no IID), so the only way to prove it works
# is to register a fake plugin interface and ask the registry. No .so, no disk,
# no widgets — hence GUILESS (QCoreApplication) and a FrameworkCore link only.

find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Test Core REQUIRED)

set(CMAKE_AUTOMOC ON)

add_executable(status_bar_discovery_tests
    test_status_bar_discovery.cpp
)

target_include_directories(status_bar_discovery_tests PRIVATE
    ${DAQSTER_SOURCE_DIR}/src/plugins/common
)

target_link_libraries(status_bar_discovery_tests PRIVATE
    FrameworkCore
    Qt${QT_VERSION_MAJOR}::Test
    Qt${QT_VERSION_MAJOR}::Core
)

add_test(NAME status_bar_discovery_tests COMMAND $<TARGET_FILE:status_bar_discovery_tests>)
