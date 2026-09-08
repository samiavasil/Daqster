cmake_minimum_required(VERSION 3.16)

project(NodeRunner VERSION 0.3.2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

add_executable(NodeRunner
    main.cpp
)

target_link_libraries(NodeRunner PRIVATE
    FrameworkCore
    NodeEditorIde
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    # Node models construct their widgets in their constructors, so the
    # runner binary needs QtWidgets (on offscreen platform for headless).
    Qt${QT_VERSION_MAJOR}::Widgets
)

# Install
install(TARGETS NodeRunner
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

message(STATUS "Created executable: NodeRunner (GUI runtime + headless via --headless flag)")