cmake_minimum_required(VERSION 3.16)

project(DaqsterCore VERSION 0.3.2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# ── DaqsterCore: QtCore-only library ──────────────────────────────────
# Contains: plugin infrastructure, platform abstraction, logging, process,
# perf, capabilities (INodeProvider, IStoppable, IStartable), NodeData types,
# and headless engine (FlowLoader, HeadlessEngine).
# NO QtWidgets, NO QtGui.

set(DAQSTER_CORE_SOURCES
    # Plugin infrastructure
    plugin/QPluginManager.cpp
    plugin/QPluginInterface.cpp
    plugin/QBasePluginObject.cpp
    plugin/QPluginLoaderExt.cpp
    plugin/discovery/PluginDiscovery.cpp
    plugin/registry/PluginRegistry.cpp
    plugin/persistence/PluginPersistence.cpp
    plugin/PluginDescription.cpp
    plugin/PluginFilter.cpp

    # Platform abstraction
    platform/ShutdownHandlerFactory.cpp
    platform/UnixShutdownHandler.cpp
    platform/WindowsShutdownHandler.cpp

    # Logging
    logging/LogManager.cpp
    logging/LogCategories.cpp

    # Process management
    process/QProcessManager.cpp

    # Performance profiling
    perf/PerfProfiler.cpp
    perf/ProcessCpu.cpp

    # Capabilities (INodeProvider is a pure interface, no .cpp needed)

    # NodeData types (all header-only, no .cpp files needed)

    # Engine (headless)
    engine/FlowLoader.cpp
    engine/HeadlessEngine.cpp
)

if(WIN32)
    list(APPEND DAQSTER_CORE_SOURCES
        platform/WindowsShutdownHandler.h
        platform/WindowsShutdownHandler.cpp
    )
else()
    list(APPEND DAQSTER_CORE_SOURCES
        platform/UnixShutdownHandler.h
        platform/UnixShutdownHandler.cpp
    )
endif()

# Headers (for installation and IDE)
set(DAQSTER_CORE_HEADERS
    plugin/QPluginManager.h
    plugin/QPluginInterface.h
    plugin/QBasePluginObject.h
    plugin/QPluginLoaderExt.h
    plugin/PluginDescription.h
    plugin/PluginFilter.h
    plugin/discovery/PluginDiscovery.h
    plugin/registry/PluginRegistry.h
    plugin/persistence/PluginPersistence.h
    plugin/include/plugin_global.h
    plugin/include/build_cfg.h
    plugin/include/debug.h

    platform/ShutdownHandler.h
    platform/ShutdownHandlerFactory.h
    platform/UnixShutdownHandler.h
    platform/WindowsShutdownHandler.h

    logging/LogManager.h
    logging/LogCategories.h

    process/QProcessManager.h

    perf/PerfProfiler.h
    perf/ProcessCpu.h

    capabilities/INodeProvider.h
    capabilities/IStoppable.h
    capabilities/IStartable.h

    data/TextData.h
    data/FloatData.h
    data/EmbeddingData.h
    data/SampledData.h
    data/SampledStreamDescriptor.h
    data/VideoFrameData.h
    data/VideoTextureHandle.h
    data/NumericType.h

    engine/FlowLoader.h
    engine/HeadlessEngine.h
)

add_library(DaqsterCore SHARED ${DAQSTER_CORE_SOURCES})

target_compile_definitions(DaqsterCore PRIVATE DAQSTER_CORE_LIBRARY)
target_compile_definitions(DaqsterCore PUBLIC DAQSTER_ENABLE_PERF=$<BOOL:${DAQSTER_ENABLE_PERF}>)

if(WIN32)
    set_target_properties(DaqsterCore PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()

target_include_directories(DaqsterCore
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/plugin
        ${CMAKE_CURRENT_SOURCE_DIR}/plugin/include
        ${CMAKE_CURRENT_SOURCE_DIR}/platform
        ${CMAKE_CURRENT_SOURCE_DIR}/logging
        ${CMAKE_CURRENT_SOURCE_DIR}/process
        ${CMAKE_CURRENT_SOURCE_DIR}/perf
        ${CMAKE_CURRENT_SOURCE_DIR}/capabilities
        ${CMAKE_CURRENT_SOURCE_DIR}/data
        ${CMAKE_CURRENT_SOURCE_DIR}/engine
        ${DAQSTER_SOURCE_DIR}/src/plugins/common
        ${DAQSTER_VERSION_INCLUDE_DIR}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/plugin/discovery
        ${CMAKE_CURRENT_SOURCE_DIR}/plugin/registry
        ${CMAKE_CURRENT_SOURCE_DIR}/plugin/persistence
)

target_link_libraries(DaqsterCore PUBLIC Qt${QT_VERSION_MAJOR}::Core QtNodes)

# QtMultimedia is optional — link if available (needed for VideoFrameData)
if(TARGET Qt${QT_VERSION_MAJOR}::Multimedia)
    target_link_libraries(DaqsterCore PUBLIC Qt${QT_VERSION_MAJOR}::Multimedia)
    target_compile_definitions(DaqsterCore PUBLIC DAQSTER_HAVE_QTMULTIMEDIA)
endif()

# QtOpenGL is optional — link if available (needed for VideoFrameData)
if(TARGET Qt${QT_VERSION_MAJOR}::OpenGL)
    target_link_libraries(DaqsterCore PUBLIC Qt${QT_VERSION_MAJOR}::OpenGL)
endif()

# Install
install(TARGETS DaqsterCore
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(FILES ${DAQSTER_CORE_HEADERS}
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/DaqsterCore
)

message(STATUS "Created library: DaqsterCore (QtCore only)")