# Unit tests for the control-plane router + schema builder (REQ-SW-PL-063).
#
# ControlPlaneRouter is the transport-independent seam shared by the HTTP and
# WebSocket paths; NodeSchemaBuilder is the enrichment merge. Both are pure
# QtCore code, so the test runs headless. ControlPlaneRouter is exported by
# FrameworkCore, hence the link (it also provides the generated export header).

find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Test Core REQUIRED)

set(CMAKE_AUTOMOC ON)

add_executable(control_plane_router_tests
    test_control_plane_router.cpp
)

target_include_directories(control_plane_router_tests PRIVATE
    ${DAQSTER_SOURCE_DIR}/src/frame_work/base/src/control
)

target_link_libraries(control_plane_router_tests PRIVATE
    FrameworkCore
    Qt${QT_VERSION_MAJOR}::Test
    Qt${QT_VERSION_MAJOR}::Core
)

add_test(NAME control_plane_router_tests COMMAND $<TARGET_FILE:control_plane_router_tests>)
