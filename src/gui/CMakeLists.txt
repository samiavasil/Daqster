cmake_minimum_required(VERSION 3.16)

project(DaqsterGui VERSION 0.3.2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ── DaqsterGui: QtWidgets library ─────────────────────────────────────
# Contains: GUI plugin manager, node editor widgets, built-in nodes,
# and merged NodeEditorLibrary content.
# Links: DaqsterCore + QtWidgets/Gui/Charts/Multimedia/Network/OpenGL

set(DAQSTER_GUI_SOURCES
    # Plugin GUI
    plugin/QPluginManagerGui.cpp
    plugin/QPluginListView.cpp
    plugin/PluginDetails.cpp
    plugin/DebugConsoleWidget.cpp

    # Editor
    editor/NodeEditorWidget.cpp
    editor/CustomDataFlowScene.cpp
    editor/NodeEditorIdeObject.cpp
    editor/RuntimeShell.cpp
    editor/FlowUiSection.cpp

    # Built-in nodes
    builtin_nodes/Sources/NumberSource/NumberSourceDataModel.cpp
    builtin_nodes/Sources/NumberSource/NumberSourceDataUi.cpp
    builtin_nodes/Displays/NumberDisplay/NumberDisplayDataModel.cpp
    builtin_nodes/Operators/Modulo/ModuloModel.cpp
    builtin_nodes/Operators/ArithmeticLogic/ArithmeticLogicModel.cpp
    builtin_nodes/Operators/ArithmeticLogic/ExprParser.cpp

    # Library (merged NodeEditorLibrary content)
    library/types/GenericNumericTypes.cpp
    library/connectors/NodeDataModelToQIODeviceConnectorObsolete.cpp
    library/connectors/GenericQDevIoConnectorObsolete.cpp
    library/decoders/AudioFrameDecoder.cpp
    library/display/QDevIoDisplayModelObsolete.cpp
    library/display/QDevioDisplayModelUiObsolete.cpp
    library/display/XYSeriesIODeviceObsolete.cpp
    library/threading/EventThreadPullObsolete.cpp
)

set(DAQSTER_GUI_HEADERS
    plugin/QPluginManagerGui.h
    plugin/QPluginListView.h
    plugin/PluginDetails.h
    plugin/DebugConsoleWidget.h

    editor/NodeEditorWidget.h
    editor/CustomDataFlowScene.h
    editor/NodeEditorIdeObject.h
    editor/RuntimeShell.h
    editor/FlowUiSection.h

    builtin_nodes/Sources/NumberSource/NumberSourceDataModel.h
    builtin_nodes/Sources/NumberSource/NumberSourceDataUi.h
    builtin_nodes/Displays/NumberDisplay/NumberDisplayDataModel.h
    builtin_nodes/Operators/Modulo/ModuloModel.h
    builtin_nodes/Operators/ArithmeticLogic/ArithmeticLogicModel.h
    builtin_nodes/Operators/ArithmeticLogic/ExprParser.h

    library/types/GenericNumericTypes.h
    library/types/IStreamDecoderObsolete.h
    library/types/QDevIOStreamConfigObsolete.h
    library/types/StreamChannelObsolete.h
    library/types/ChatGraphModel.h
    library/connectors/NodeDataModelToQIODeviceConnectorObsolete.h
    library/connectors/GenericQDevIoConnectorObsolete.h
    library/decoders/AudioFrameDecoder.h
    library/display/QDevIoDisplayModelObsolete.h
    library/display/QDevioDisplayModelUiObsolete.h
    library/display/XYSeriesIODeviceObsolete.h
    library/display/AudioCompat.h
    library/display/QtChartsCompat.h
    library/threading/EventThreadPullObsolete.h
)

set(DAQSTER_GUI_UI_FILES
    plugin/pluginmanagergui.ui
    plugin/pluginlistview.ui
    plugin/plugindetails.ui
    builtin_nodes/Sources/NumberSource/NumberSourceDataUi.ui
    library/display/QDevioDisplayModelUiObsolete.ui
)

set(DAQSTER_GUI_RESOURCES
    node_editor.qrc
)

add_library(DaqsterGui SHARED ${DAQSTER_GUI_SOURCES} ${DAQSTER_GUI_HEADERS} ${DAQSTER_GUI_UI_FILES} ${DAQSTER_GUI_RESOURCES})

target_compile_definitions(DaqsterGui PRIVATE DAQSTER_GUI_LIBRARY)
target_compile_definitions(DaqsterGui PUBLIC DAQSTER_ENABLE_PERF=$<BOOL:${DAQSTER_ENABLE_PERF}>)

if(WIN32)
    set_target_properties(DaqsterGui PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()

target_include_directories(DaqsterGui
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/plugin
        ${CMAKE_CURRENT_SOURCE_DIR}/editor
        ${CMAKE_CURRENT_SOURCE_DIR}/builtin_nodes
        ${CMAKE_CURRENT_SOURCE_DIR}/library
        ${DAQSTER_SOURCE_DIR}/src/core
        ${DAQSTER_VERSION_INCLUDE_DIR}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/builtin_nodes/Sources/NumberSource
        ${CMAKE_CURRENT_SOURCE_DIR}/builtin_nodes/Displays/NumberDisplay
        ${CMAKE_CURRENT_SOURCE_DIR}/builtin_nodes/Operators/Modulo
        ${CMAKE_CURRENT_SOURCE_DIR}/builtin_nodes/Operators/ArithmeticLogic
        ${CMAKE_CURRENT_SOURCE_DIR}/library/types
        ${CMAKE_CURRENT_SOURCE_DIR}/library/connectors
        ${CMAKE_CURRENT_SOURCE_DIR}/library/decoders
        ${CMAKE_CURRENT_SOURCE_DIR}/library/display
        ${CMAKE_CURRENT_SOURCE_DIR}/library/threading
)

target_link_libraries(DaqsterGui PUBLIC
    DaqsterCore
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Widgets
    QtNodes
)

# QtCharts is optional — link if available
if(TARGET Qt${QT_VERSION_MAJOR}::Charts)
    target_link_libraries(DaqsterGui PUBLIC Qt${QT_VERSION_MAJOR}::Charts)
endif()

# QtMultimedia is optional — link if available
if(TARGET Qt${QT_VERSION_MAJOR}::Multimedia)
    target_link_libraries(DaqsterGui PUBLIC Qt${QT_VERSION_MAJOR}::Multimedia)
endif()

# QtMultimediaWidgets is optional — link if available
if(TARGET Qt${QT_VERSION_MAJOR}::MultimediaWidgets)
    target_link_libraries(DaqsterGui PUBLIC Qt${QT_VERSION_MAJOR}::MultimediaWidgets)
endif()

# QtNetwork is optional — link if available
if(TARGET Qt${QT_VERSION_MAJOR}::Network)
    target_link_libraries(DaqsterGui PUBLIC Qt${QT_VERSION_MAJOR}::Network)
endif()

# QtOpenGL is optional — link if available
if(TARGET Qt${QT_VERSION_MAJOR}::OpenGL)
    target_link_libraries(DaqsterGui PUBLIC Qt${QT_VERSION_MAJOR}::OpenGL)
endif()

# Install
install(TARGETS DaqsterGui
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(FILES ${DAQSTER_GUI_HEADERS}
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/DaqsterGui
)

message(STATUS "Created library: DaqsterGui (QtWidgets)")