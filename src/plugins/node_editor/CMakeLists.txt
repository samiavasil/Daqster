# Use Qt detection from parent scope
#add_subdirectory(external)

add_library(NodeEditorPlugin
SHARED
    NodeEditorPluginObject.cpp
    NodeEditorInterface.cpp
    Audio/AudioNodeQdevIoConnector.cpp
    Audio/AudioSourceConfig.cpp
    Audio/AudioSourceDataModel.cpp
    Audio/AudioSourceDataModelUI.cpp
    Audio/AudioWorker.cpp
    Library/NodeDataModelToQIODeviceConnector.cpp
    Examples/Converters.cpp
    Examples/ModuloModel.cpp
    Examples/NumberDisplayDataModel.cpp
    Examples/NumberSourceDataModel.cpp
    Examples/NumberSourceDataUi.cpp
    ThreadPull/EventThreadPull.cpp
    Library/QDevIoDisplayModel.cpp
    Library/QDevioDisplayModelUi.cpp
    Library/XYSeriesIODevice.cpp
    NodeEditorInterface.h
    NodeEditorPluginObject.h
    Audio/AudioNodeQdevIoConnector.h
    Audio/AudioSourceConfig.h
    Audio/AudioSourceDataModel.h
    Audio/AudioSourceDataModelUI.h
    Audio/AudioWorker.h
    Library/NodeDataModelToQIODeviceConnector.h
    Examples/Converters.h
    Examples/ModuloModel.h
    Examples/NumberDisplayDataModel.h
    Examples/NumberSourceDataModel.h
    Examples/NumberSourceDataUi.h
    Examples/Converters.h
    Examples/DecimalData.h
    Examples/IntegerData.h
    Examples/ModuloModel.h
    ThreadPull/EventThreadPull.h
    Library/ComplexType.h
    Library/QDevIoDisplayModel.h
    Library/QDevioDisplayModelUi.h
    Library/XYSeriesIODevice.h
    Audio/AudioSourceConfig.ui
    Audio/AudioSourceDataModelUI.ui
    Examples/NumberSourceDataUi.ui
    Library/QDevioDisplayModelUi.ui
    Audio/node_editor.qrc
)

add_definitions(-DBUILD_AVAILABLE_PLUGIN
    -DNODE_EDITOR_SHARED)

target_include_directories(NodeEditorPlugin
  PRIVATE
    ./
    ../../external_libs/nodeeditor/include
    ./Audio
    ./Library
    ./Examples
    ./ThreadPull
  )
target_link_libraries(NodeEditorPlugin
    frame_work
    nodes
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Multimedia
    Qt${QT_VERSION_MAJOR}::Charts
    Qt${QT_VERSION_MAJOR}::OpenGL)

# Install plugin to dedicated directory
install(TARGETS NodeEditorPlugin
    RUNTIME DESTINATION bin/plugins
    LIBRARY DESTINATION lib/daqster/plugins
    ARCHIVE DESTINATION lib
)

# Ensure plugin can locate shared libs in ../.. /lib relative to its location
set_target_properties(NodeEditorPlugin PROPERTIES
    INSTALL_RPATH "$ORIGIN/../../lib"
)
