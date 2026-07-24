# frame_work_gui - GUI components for the Daqster framework
# Depends on frame_work (core library)

option(FRAME_WORK_GUI_BUILD_STATIC "Build frame_work_gui as a static library" OFF)

set(FRAME_WORK_GUI_SOURCES
  base/src/QPluginManagerGui.h
  base/src/QPluginManagerGui.cpp
  base/src/QPluginListView.h
  base/src/QPluginListView.cpp
  base/src/PluginDetails.h
  base/src/PluginDetails.cpp
  base/src/pluginmanagergui.ui
  base/src/pluginlistview.ui
  base/src/plugindetails.ui
  framework_resources.qrc
)

if(FRAME_WORK_GUI_BUILD_STATIC)
  add_library(frame_work_gui STATIC
    ${FRAME_WORK_GUI_SOURCES}
  )
  target_compile_definitions(frame_work_gui PUBLIC FRAME_WORK_STATIC)
else()
  add_library(frame_work_gui SHARED
    ${FRAME_WORK_GUI_SOURCES}
  )
  target_compile_definitions(frame_work_gui PRIVATE FRAME_WORK_LIBRARY)

  if(WIN32)
    set_target_properties(frame_work_gui PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
  endif()
endif()

target_include_directories(frame_work_gui
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/base/src
  PRIVATE
    ${DAQSTER_SOURCE_DIR}/src/frame_work/base/src
    ${DAQSTER_SOURCE_DIR}/src/frame_work/base/src/include
)

target_link_libraries(frame_work_gui
    frame_work
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Gui
)

# Install shared library
install(TARGETS frame_work_gui
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
