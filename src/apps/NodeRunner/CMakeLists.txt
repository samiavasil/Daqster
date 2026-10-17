cmake_minimum_required(VERSION 3.16)

project(NodeRunner VERSION 0.3.2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# REQ-SW-PL-053 / REQ-SW-PL-072: the runner is mode-agnostic. It links
# FrameworkCore + QtCore only — never QtWidgets (nor QtGui) — and resolves the
# engine for the requested runtime mode at startup through
# QPluginManager::runtimeHosts(), so no concrete plugin type is referenced from
# application code and the GUI engine arrives purely as a dlopen()ed .so.
#
#   NodeRunner --run <flow.flow>            -> RuntimeHostGuiPlugin  (RuntimeShell)
#   NodeRunner --headless --run <flow.flow>  -> RuntimeHostHeadlessPlugin (HeadlessEngine)
#
# The application object (the only genuine QtWidgets user) is not built here:
# it comes from a small launcher library loaded at runtime (REQ-SW-PL-072) —
# DaqsterLauncherHeadless (QGuiApplication, Qt::Gui) for --headless and
# DaqsterLauncherGui (QApplication, QtWidgets) otherwise. Both set
# Qt::AA_ShareOpenGLContexts so GPU/GL optimisations work in both modes.
add_executable(NodeRunner
    main.cpp
)
target_link_libraries(NodeRunner PRIVATE
    FrameworkCore
    Qt${QT_VERSION_MAJOR}::Core
)
target_compile_definitions(NodeRunner PRIVATE
    DAQSTER_RUNNER_TARGET_NAME="NodeRunner"
    DAQSTER_LAUNCHER_HEADLESS_NAME="DaqsterLauncherHeadless"
    DAQSTER_LAUNCHER_GUI_NAME="DaqsterLauncherGui"
)

# Capability interfaces (INodeProvider, IRuntimeHost) — headers only.
target_include_directories(NodeRunner PRIVATE
    ${DAQSTER_SOURCE_DIR}/src/plugins/common
)

install(TARGETS NodeRunner
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

message(STATUS "Created executable: NodeRunner (single app, launcher-selected headless/GUI runtime)")
