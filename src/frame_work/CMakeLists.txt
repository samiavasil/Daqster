# Use Qt detection from parent scope

option(FRAME_WORK_BUILD_STATIC "Build frame_work as a static library" OFF)

set(FRAME_WORK_SOURCES
  base/src/global.h
  base/src/PluginDescription.cpp
  base/src/PluginFilter.cpp
  base/src/QBasePluginObject.cpp
  base/src/QPluginInterface.cpp
  base/src/LogCategories.cpp
  base/src/LogManager.cpp
  base/src/QPluginLoaderExt.cpp
  base/src/QPluginLoaderExt.h
  base/src/QPluginManager.cpp

  base/src/include/QPluginManager.h
  base/src/include/QPluginInterface.h
  base/src/include/QBasePluginObject.h
  base/src/include/LogCategories.h
  base/src/include/LogManager.h
  base/src/platform/ShutdownHandler.h
  base/src/platform/ShutdownHandlerFactory.cpp
  base/src/process/QProcessManager.h
  base/src/process/QProcessManager.cpp
  base/src/discovery/PluginDiscovery.h
  base/src/discovery/PluginDiscovery.cpp
  base/src/persistence/PluginPersistence.h
  base/src/persistence/PluginPersistence.cpp
  base/src/registry/PluginRegistry.h
  base/src/registry/PluginRegistry.cpp
  base/src/gui/QPluginManagerGui.h
  base/src/gui/QPluginManagerGui.cpp
  base/src/gui/QPluginListView.h
  base/src/gui/QPluginListView.cpp
  base/src/gui/PluginDetails.h
  base/src/gui/PluginDetails.cpp
  base/src/gui/DebugConsoleWidget.cpp
  base/src/gui/DebugConsoleWidget.h
  base/src/gui/pluginmanagergui.ui
  base/src/gui/pluginlistview.ui
  base/src/gui/plugindetails.ui
  framework_resources.qrc
)

if(WIN32)
  list(APPEND FRAME_WORK_SOURCES
    base/src/platform/WindowsShutdownHandler.h
    base/src/platform/WindowsShutdownHandler.cpp
  )
else()
  list(APPEND FRAME_WORK_SOURCES
    base/src/platform/UnixShutdownHandler.h
    base/src/platform/UnixShutdownHandler.cpp
  )
endif()

if(FRAME_WORK_BUILD_STATIC)
  # Static build of frame_work: no dllimport/dllexport, all
  # symbols are linked directly into consumers.
  add_library(frame_work STATIC
    ${FRAME_WORK_SOURCES}
  )
  target_compile_definitions(frame_work PUBLIC FRAME_WORK_STATIC)
else()
  # Default/shared build: provide a shared framework library
  # with proper export/import decorations.
  add_library(frame_work SHARED
    ${FRAME_WORK_SOURCES}
  )
  target_compile_definitions(frame_work PRIVATE FRAME_WORK_LIBRARY)

  # On Windows, also auto-export all public symbols from the
  # frame_work DLL so that classes without explicit
  # FRAME_WORKSHARED_EXPORT (e.g. QProcessManager,
  # ShutdownHandler) are visible to Daqster.exe and plugins.
  if(WIN32)
    set_target_properties(frame_work PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
  endif()
endif()

target_include_directories(frame_work
  PUBLIC
    base/src/include
    base/src/platform
    base/src/process
    base/src/discovery
    base/src/persistence
    base/src/registry
    base/src/gui
  PRIVATE
    base/src
)

target_link_libraries(frame_work 
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Widgets
)

# Install shared library (and headers for consumers if needed)
install(TARGETS frame_work
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
