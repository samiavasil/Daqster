include(GenerateExportHeader)

# Daqster framework core/gui split (REQ-SW-PL-051)
#
# This directory owns the two framework libraries:
#
#   DaqsterCore - QtCore + QtNodes (+ optional QtMultimedia / QtOpenGL)
#                 plugin infrastructure, discovery, registry, persistence,
#                 logging, platform abstraction, process management,
#                 performance profiling and the headless engine.
#
#   DaqsterGui  - QtWidgets (layered on top of DaqsterCore)
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

option(DAQSTER_BUILD_STATIC "Build DaqsterCore/DaqsterGui as static libraries" OFF)
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
# DaqsterCore
# ═══════════════════════════════════════════════════════════════════════════
set(DAQSTER_CORE_SOURCES
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
    base/src/engine/FlowLoader.cpp
    base/src/engine/FlowLoader.h
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
    add_library(DaqsterCore STATIC ${DAQSTER_CORE_SOURCES})
    target_compile_definitions(DaqsterCore PUBLIC DAQSTER_CORE_STATIC)
else()
    add_library(DaqsterCore SHARED ${DAQSTER_CORE_SOURCES})
    target_compile_definitions(DaqsterCore PRIVATE DAQSTER_CORE_LIBRARY)
endif()

target_compile_definitions(DaqsterCore PUBLIC
    DAQSTER_ENABLE_PERF=$<BOOL:${DAQSTER_ENABLE_PERF}>
)

set_target_properties(DaqsterCore PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
    POSITION_INDEPENDENT_CODE ON
)

# daqster_core_export.h is generated here and exposed publicly so that every
# plugin and application receives DAQSTER_CORE_EXPORT through the link.
generate_export_header(DaqsterCore
    BASE_NAME daqster_core
    EXPORT_FILE_NAME ${CMAKE_CURRENT_BINARY_DIR}/daqster_core_export.h
)

target_include_directories(DaqsterCore
    PUBLIC
        ${DAQSTER_FRAMEWORK_INCLUDE_DIRS}
        ${CMAKE_CURRENT_SOURCE_DIR}/base/src/discovery
        ${CMAKE_CURRENT_SOURCE_DIR}/base/src/registry
        ${CMAKE_CURRENT_SOURCE_DIR}/base/src/persistence
        ${CMAKE_CURRENT_SOURCE_DIR}/base/src/engine
        ${CMAKE_CURRENT_SOURCE_DIR}/base/src/platform
        ${CMAKE_CURRENT_SOURCE_DIR}/base/src/process
        ${CMAKE_CURRENT_SOURCE_DIR}/base/src/perf
        ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor_ide/BuiltInNodes
        ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor_ide/BuiltInNodes/Sources
        ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor_ide/BuiltInNodes/Displays
        ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor_ide/BuiltInNodes/Operators
        ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor_ide/BuiltInNodes/Library
        ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor_ide/BuiltInNodes/Library/types
        ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor_ide/BuiltInNodes/Library/display
        ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor_ide/BuiltInNodes/Library/connectors
        ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor_ide/BuiltInNodes/Library/decoders
        ${DAQSTER_SOURCE_DIR}/src/plugins/node_editor_ide/BuiltInNodes/Library/threading
)

target_link_libraries(DaqsterCore
    PUBLIC
        Qt${QT_VERSION_MAJOR}::Core
        QtNodes
)

# QtMultimedia / QtOpenGL are optional but required by the shared NodeData
# types (VideoFrameData / VideoTextureHandle) that live in plugins/common.
if(TARGET Qt${QT_VERSION_MAJOR}::Multimedia)
    target_link_libraries(DaqsterCore PUBLIC Qt${QT_VERSION_MAJOR}::Multimedia)
    target_compile_definitions(DaqsterCore PUBLIC DAQSTER_HAVE_QTMULTIMEDIA)
endif()

if(TARGET Qt${QT_VERSION_MAJOR}::OpenGL)
    target_link_libraries(DaqsterCore PUBLIC Qt${QT_VERSION_MAJOR}::OpenGL)
endif()

install(TARGETS DaqsterCore
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/daqster_core_export.h
    ${CMAKE_CURRENT_SOURCE_DIR}/base/src/include/QPluginManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/base/src/include/QPluginInterface.h
    ${CMAKE_CURRENT_SOURCE_DIR}/base/src/include/QBasePluginObject.h
    ${CMAKE_CURRENT_SOURCE_DIR}/base/src/include/QPluginLoaderExt.h
    ${CMAKE_CURRENT_SOURCE_DIR}/base/src/include/PluginDescription.h
    ${CMAKE_CURRENT_SOURCE_DIR}/base/src/include/PluginFilter.h
    ${CMAKE_CURRENT_SOURCE_DIR}/base/src/include/plugin_global.h
    ${CMAKE_CURRENT_SOURCE_DIR}/base/src/include/debug.h
    ${CMAKE_CURRENT_SOURCE_DIR}/base/src/include/build_cfg.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/DaqsterCore
)

message(STATUS "Created library: DaqsterCore (QtCore + QtNodes)")

# ═══════════════════════════════════════════════════════════════════════════
# DaqsterGui
# ═══════════════════════════════════════════════════════════════════════════
if(DAQSTER_BUILD_GUI)
    set(DAQSTER_GUI_SOURCES
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
        add_library(DaqsterGui STATIC ${DAQSTER_GUI_SOURCES})
        target_compile_definitions(DaqsterGui PUBLIC DAQSTER_GUI_STATIC)
    else()
        add_library(DaqsterGui SHARED ${DAQSTER_GUI_SOURCES})
        target_compile_definitions(DaqsterGui PRIVATE DAQSTER_GUI_LIBRARY)
    endif()

    target_compile_definitions(DaqsterGui PUBLIC
        DAQSTER_ENABLE_PERF=$<BOOL:${DAQSTER_ENABLE_PERF}>
    )

    set_target_properties(DaqsterGui PROPERTIES
        AUTOMOC ON
        AUTOUIC ON
        AUTORCC ON
        POSITION_INDEPENDENT_CODE ON
    )

    # daqster_gui_export.h is generated here and exposed publicly.
    generate_export_header(DaqsterGui
        BASE_NAME daqster_gui
        EXPORT_FILE_NAME ${CMAKE_CURRENT_BINARY_DIR}/daqster_gui_export.h
    )

    target_include_directories(DaqsterGui
        PUBLIC
            ${DAQSTER_FRAMEWORK_INCLUDE_DIRS}
            ${CMAKE_CURRENT_SOURCE_DIR}/base/src/gui
    )

    target_link_libraries(DaqsterGui
        PUBLIC
            DaqsterCore
            Qt${QT_VERSION_MAJOR}::Core
            Qt${QT_VERSION_MAJOR}::Gui
            Qt${QT_VERSION_MAJOR}::Widgets
            QtNodes
    )

    # Optional Qt modules used by the framework GUI widgets.
    foreach(DAQSTER_GUI_OPTIONAL_MODULE Charts Multimedia MultimediaWidgets Network OpenGL)
        if(TARGET Qt${QT_VERSION_MAJOR}::${DAQSTER_GUI_OPTIONAL_MODULE})
            target_link_libraries(DaqsterGui PUBLIC Qt${QT_VERSION_MAJOR}::${DAQSTER_GUI_OPTIONAL_MODULE})
        endif()
    endforeach()

    install(TARGETS DaqsterGui
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )

    install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/daqster_gui_export.h
        ${CMAKE_CURRENT_SOURCE_DIR}/base/src/gui/QPluginManagerGui.h
        ${CMAKE_CURRENT_SOURCE_DIR}/base/src/gui/QPluginListView.h
        ${CMAKE_CURRENT_SOURCE_DIR}/base/src/gui/PluginDetails.h
        ${CMAKE_CURRENT_SOURCE_DIR}/base/src/gui/DebugConsoleWidget.h
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/DaqsterGui
    )

    message(STATUS "Created library: DaqsterGui (QtWidgets)")
endif()
