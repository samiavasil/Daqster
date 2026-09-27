cmake_minimum_required(VERSION 3.16)

project(NodeRunner VERSION 0.3.2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# REQ-SW-PL-053: the runner is mode-agnostic. It links FrameworkCore only and
# resolves the engine for the requested runtime mode at startup through
# QPluginManager::runtimeHosts(), so no concrete plugin type is referenced from
# application code and the GUI engine arrives purely as a dlopen()ed .so.
#
#   NodeRunner --run <flow.flow>            -> FrameworkGuiPlugin  (RuntimeShell)
#   NodeRunner --headless --run <flow.flow>  -> FrameworkCorePlugin (HeadlessEngine)
#
# QtWidgets is on the link line because the GUI mode needs QApplication, and in
# Qt5 QApplication itself lives in QtWidgets (it moved to QtGui only in Qt6).
# A QtWidgets-free headless binary is therefore not reachable yet: node models
# still build their widgets in their constructors, so even a QGuiApplication
# headless build aborts in QWidgetPrivate::init(). See REQ-SW-PL-053 AC6.
add_executable(NodeRunner
    main.cpp
)
target_link_libraries(NodeRunner PRIVATE
    FrameworkCore
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Widgets
)
target_compile_definitions(NodeRunner PRIVATE
    DAQSTER_RUNNER_TARGET_NAME="NodeRunner"
)

# Capability interfaces (INodeProvider, IRuntimeHost) — headers only.
target_include_directories(NodeRunner PRIVATE
    ${DAQSTER_SOURCE_DIR}/src/plugins/common
)

install(TARGETS NodeRunner
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

message(STATUS "Created executable: NodeRunner (single app, plugin-selected headless/GUI runtime)")
