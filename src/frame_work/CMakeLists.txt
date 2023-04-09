find_package(Qt5 COMPONENTS widgets REQUIRED)

add_library(frame_work
    SHARED
        base/src/global.h
        base/src/PluginDescription.cpp
        base/src/PluginDetails.cpp
        base/src/PluginDetails.h
        base/src/PluginFilter.cpp
        base/src/QBasePluginObject.cpp
        base/src/QPluginInterface.cpp
        base/src/QPluginListView.cpp
        base/src/QPluginListView.h
        base/src/QPluginLoaderExt.cpp
        base/src/QPluginLoaderExt.h
        base/src/QPluginManager.cpp
        base/src/QPluginManagerGui.cpp
        base/src/QPluginManagerGui.h
        base/src/Singleton.h
        base/src/pluginmanagergui.ui
        base/src/pluginlistview.ui
        base/src/plugindetails.ui
        framework_resources.qrc
        base/src/include/QPluginManager.h
        base/src/include/QPluginInterface.h
        base/src/include/QBasePluginObject.h
)

add_definitions(-DFRAME_WORK_LIBRARY)

target_include_directories(frame_work
  PUBLIC
    base/src/include
  )
target_link_libraries(frame_work Qt5::Widgets)
