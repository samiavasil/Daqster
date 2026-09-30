include(GenerateExportHeader)

# Daqster framework core/gui split (REQ-SW-PL-051)
#
# This directory owns the two framework libraries:
#
#   FrameworkCore - QtCore + QtNodes (+ optional QtMultimedia / QtOpenGL)
#                 plugin infrastructure, discovery, registry, persistence,
#                 logging, platform abstraction, process management,
#                 performance profiling and the headless engine.
#
#   FrameworkGui  - QtWidgets (layered on top of FrameworkCore)
#                 the framework plugin-manager widgets and their .ui forms.
#
# Node/editor/NodeData conventions do NOT live here - they belong to
# src/plugins/common and the node_editor_ide application plugin.
#
# A single build configuration drives both libraries: either both are shared
# (default) or both are static (DAQSTER_BUILD_STATIC=ON). There is no second
# copy of the export configuration anywhere in the tree.

# ── Qt detection and variables are inherited from the parent scope ───────────
# QT_VERSION_MAJOR, DAQSTER_VERSION, DAQSTER_VERSION_INCLUDE_DIR

option(DAQSTER_BUILD_STATIC "Build FrameworkCore/FrameworkGui as static libraries" OFF)
option(DAQSTER_ENABLE_PERF "Enable lightweight runtime profiling" ON)

# QtNodes is the external "nodeeditor" component; the headless engine uses its
# DataFlowGraphModel/registry only (no graphics scene is instantiated).
set(DAQSTER_FRAMEWORK_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/base/src
    ${CMAKE_CURRENT_SOURCE_DIR}/base/src/include
    ${CMAKE_CURRENT_SOURCE_DIR}/base/src/logging
    ${DAQSTER_SOURCE_DIR}/src/plugins/common
    ${DAQSTER_VERSION_INCLUDE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
)

# ═══════════════════════════════════════════════════════════════════════════
# FrameworkCore
# ═══════════════════════════════════════════════════════════════════════════
set(FRAMEWORK_CORE_SOURCES
    # Plugin infrastructure (canonical public headers in base/src/include)
    base/src/PluginDescription.cpp
    base/src/PluginFilter.cpp
    base/src/QBasePluginObject.cpp
    base/src/QPluginInterface.cpp
    base/src/QPluginLoaderExt.cpp
    base/src/QPluginManager.cpp

    base/src/include/QPluginManager.h
    base/src/include/QPluginInterface.h
    base/src/include/QBasePluginObject.h
    base/src/include/QPluginLoaderExt.h
    base/src/include/PluginDescription.h
    base/src/include/PluginFilter.h
    base/src/include/plugin_global.h
    base/src/include/debug.h
    base/src/include/build_cfg.h
    base/src/global.h

    # Discovery / registry / persistence
    base/src/discovery/PluginDiscovery.cpp
    base/src/discovery/PluginDiscovery.h
    base/src/registry/PluginRegistry.cpp
    base/src/registry/PluginRegistry.h
    base/src/persistence/PluginPersistence.cpp
    base/src/persistence/PluginPersistence.h

    # Headless engine
    base/src/engine/HeadlessApp.cpp
    base/src/engine/HeadlessApp.h
    base/src/engine/HeadlessEngine.cpp
    base/src/engine/HeadlessEngine.h

    # Logging
    base/src/logging/LogCategories.cpp
    base/src/logging/LogCategories.h
    base/src/logging/LogManager.cpp
    base/src/logging/LogManager.h

    # Platform abstraction (factory + both backends; the factory selects)
    base/src/platform/ShutdownHandler.h
    base/src/platform/ShutdownHandlerFactory.cpp
    base/src/platform/UnixShutdownHandler.cpp
    base/src/platform/UnixShutdownHandler.h
    base/src/platform/WindowsShutdownHandler.cpp
    base/src/platform/WindowsShutdownHandler.h

    # Process management + performance profiling
    base/src/process/QProcessManager.cpp
    base/src/process/QProcessManager.h
    base/src/perf/PerfProfiler.cpp
    base/src/perf/PerfProfiler.h
    base/src/perf/ProcessCpu.cpp
    base/src/perf/ProcessCpu.h
)

if(DAQSTER_BUILD_STATIC)
    add_library(FrameworkCore STATIC ${FRAMEWORK_CORE_SOURCES})
    target_compile_definitions(FrameworkCore PUBLIC FRAMEWORK_CORE_STATIC)
else()
    add_library(FrameworkCore SHARED ${FRAMEWORK_CORE_SOURCES})
    target_compile_definitions(FrameworkCore PRIVATE FRAMEWORK_CORE_LIBRARY)
endif()

target_compile_definitions(FrameworkCore PUBLIC
    DAQSTER_ENABLE_PERF=$<BOOL:${DAQSTER_ENABLE_PERF}>
)

set_target_properties(FrameworkCore PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
    POSITION_INDEPENDENT_CODE ON
)

# framework_core_export.h is generated here and exposed publicly so that every
# plugin and application receives FRAMEWORK_CORE_EXPORT through the link.
generate_export_header(FrameworkCore
    BASE_NAME framework_core
    EXPORT_FILE_NAME ${CMAKE_CURRENT_BINARY_DIR}/framework_core_export.h
)

target_include_directories(FrameworkCore
    PUBLIC
        ${DAQSTER_FRAMEWORK_INCLUDE_DIRS}
        ${CMAKE_CURRENT_SOURCE_DIR}/base/src/discovery
        ${CMAKE_CURRENT_SOURCE_DIR}/base/src/registry
        ${CMAKE_CURRENT_SOURCE_DIR}/base/src/persistence
        ${CMAKE_CURRENT_SOURCE_DIR}/base/src/engine
        ${CMAKE_CURRENT_SOURCE_DIR}/base/src/platform
        ${CMAKE_CURRENT_SOURCE_DIR}/base/src/process
        ${CMAKE_CURRENT_SOURCE_DIR}/base/src/perf
        ${CMAKE_CURRENT_BINARY_DIR}
        ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor_ide/BuiltInNodes
        ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor_ide/BuiltInNodes/Library
        ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor_ide/BuiltInNodes/Library/types
        ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor_ide/BuiltInNodes/Library/display
        ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor_ide/BuiltInNodes/Library/connectors
        ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor_ide/BuiltInNodes/Library/decoders
        ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor_ide/BuiltInNodes/Library/threading
)

target_link_libraries(FrameworkCore
    PUBLIC
        Qt${QT_VERSION_MAJOR}::Core
        QtNodes
)

# QtMultimedia / QtOpenGL are optional but required by the shared NodeData
# types (VideoFrameData / VideoTextureHandle) that live in plugins/common.
if(TARGET Qt${QT_VERSION_MAJOR}::Multimedia)
    target_link_libraries(FrameworkCore PUBLIC Qt${QT_VERSION_MAJOR}::Multimedia)
    target_compile_definitions(FrameworkCore PUBLIC FRAMEWORK_HAVE_QTMULTIMEDIA)
endif()

if(TARGET Qt${QT_VERSION_MAJOR}::OpenGL)
    target_link_libraries(FrameworkCore PUBLIC Qt${QT_VERSION_MAJOR}::OpenGL)
endif()

install(TARGETS FrameworkCore
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/framework_core_export.h
    ${CMAKE_CURRENT_SOURCE_DIR}/base/src/include/QPluginManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/base/src/include/QPluginInterface.h
    ${CMAKE_CURRENT_SOURCE_DIR}/base/src/include/QBasePluginObject.h
    ${CMAKE_CURRENT_SOURCE_DIR}/base/src/include/QPluginLoaderExt.h
    ${CMAKE_CURRENT_SOURCE_DIR}/base/src/include/PluginDescription.h
    ${CMAKE_CURRENT_SOURCE_DIR}/base/src/include/PluginFilter.h
    ${CMAKE_CURRENT_SOURCE_DIR}/base/src/include/plugin_global.h
    ${CMAKE_CURRENT_SOURCE_DIR}/base/src/include/debug.h
    ${CMAKE_CURRENT_SOURCE_DIR}/base/src/include/build_cfg.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/FrameworkCore
)

message(STATUS "Created library: FrameworkCore (QtCore + QtNodes)")

# ═══════════════════════════════════════════════════════════════════════════
# FrameworkGui
# ═══════════════════════════════════════════════════════════════════════════
if(DAQSTER_BUILD_GUI)
    set(FRAMEWORK_GUI_SOURCES
        base/src/gui/QPluginManagerGui.cpp
        base/src/gui/QPluginManagerGui.h
        base/src/gui/QPluginListView.cpp
        base/src/gui/QPluginListView.h
        base/src/gui/PluginDetails.cpp
        base/src/gui/PluginDetails.h
        base/src/gui/DebugConsoleWidget.cpp
        base/src/gui/DebugConsoleWidget.h

        base/src/gui/pluginmanagergui.ui
        base/src/gui/pluginlistview.ui
        base/src/gui/plugindetails.ui

        framework_resources.qrc
    )

    if(DAQSTER_BUILD_STATIC)
        add_library(FrameworkGui STATIC ${FRAMEWORK_GUI_SOURCES})
        target_compile_definitions(FrameworkGui PUBLIC FRAMEWORK_GUI_STATIC)
    else()
        add_library(FrameworkGui SHARED ${FRAMEWORK_GUI_SOURCES})
        target_compile_definitions(FrameworkGui PRIVATE FRAMEWORK_GUI_LIBRARY)
    endif()

    target_compile_definitions(FrameworkGui PUBLIC
        DAQSTER_ENABLE_PERF=$<BOOL:${DAQSTER_ENABLE_PERF}>
    )

    set_target_properties(FrameworkGui PROPERTIES
        AUTOMOC ON
        AUTOUIC ON
        AUTORCC ON
        POSITION_INDEPENDENT_CODE ON
    )

    # framework_gui_export.h is generated here and exposed publicly.
    generate_export_header(FrameworkGui
        BASE_NAME framework_gui
        EXPORT_FILE_NAME ${CMAKE_CURRENT_BINARY_DIR}/framework_gui_export.h
    )

    target_include_directories(FrameworkGui
        PUBLIC
            ${DAQSTER_FRAMEWORK_INCLUDE_DIRS}
            ${CMAKE_CURRENT_SOURCE_DIR}/base/src/gui
    )

    target_link_libraries(FrameworkGui
        PUBLIC
            FrameworkCore
            Qt${QT_VERSION_MAJOR}::Core
            Qt${QT_VERSION_MAJOR}::Gui
            Qt${QT_VERSION_MAJOR}::Widgets
            QtNodes
    )

    # Optional Qt modules used by the framework GUI widgets.
    foreach(FRAMEWORK_GUI_OPTIONAL_MODULE Charts Multimedia MultimediaWidgets Network OpenGL)
        if(TARGET Qt${QT_VERSION_MAJOR}::${FRAMEWORK_GUI_OPTIONAL_MODULE})
            target_link_libraries(FrameworkGui PUBLIC Qt${QT_VERSION_MAJOR}::${FRAMEWORK_GUI_OPTIONAL_MODULE})
        endif()
    endforeach()

    install(TARGETS FrameworkGui
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )

    install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/framework_gui_export.h
        ${CMAKE_CURRENT_SOURCE_DIR}/base/src/gui/QPluginManagerGui.h
        ${CMAKE_CURRENT_SOURCE_DIR}/base/src/gui/QPluginListView.h
        ${CMAKE_CURRENT_SOURCE_DIR}/base/src/gui/PluginDetails.h
        ${CMAKE_CURRENT_SOURCE_DIR}/base/src/gui/DebugConsoleWidget.h
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/FrameworkGui
    )

    message(STATUS "Created library: FrameworkGui (QtWidgets)")
endif()
