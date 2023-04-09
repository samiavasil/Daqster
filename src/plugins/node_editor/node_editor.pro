#-------------------------------------------------
#
# Project created by QtCreator 2012-06-21T13:34:25
#
#-------------------------------------------------
include(../../include/paths_cfg.pri)
DESTDIR      = $${PLUGINS_DEST_DIR}
QT          += core gui

TEMPLATE     = lib
INCLUDEPATH += ../
INCLUDEPATH += $${FRAMEWORK_INCLUDES_DIR}
INCLUDEPATH += $${EXT_LIBS_INCLUDES_DIR}
INCLUDEPATH += $$PWD/../../external_libs/nodeeditor/include
INCLUDEPATH += $$PWD
DEPENDPATH  += $$PWD
OBJECTS_DIR  = $$PWD/Build
MOC_DIR      = $$PWD/Build
RCC_DIR      = $$PWD/Build
UI_DIR       = $$PWD/Build
LIBS        += -L$${EXT_LIBS_DIR} -L$${LIBS_DIR}

CONFIG+= c++14
QT += multimedia
QT += charts
CONFIG      += plugin
DEFINES     += BUILD_AVAILABLE_PLUGIN
DEFINES     += NODE_EDITOR_SHARED

#Fix me
INCLUDEPATH += $$PWD/Audio
INCLUDEPATH += $$PWD/Library
INCLUDEPATH += $$PWD/Examples
INCLUDEPATH += $$PWD/ThreadPull

TARGET   = NodeEditorPlugin

SOURCES += \
    NodeEditorPluginObject.cpp \
    NodeEditorInterface.cpp \
    Audio/AudioNodeQdevIoConnector.cpp \
    Audio/AudioSourceConfig.cpp \
    Audio/AudioSourceDataModel.cpp \
    Audio/AudioSourceDataModelUI.cpp \
    Audio/AudioWorker.cpp \
    Library/NodeDataModelToQIODeviceConnector.cpp \
    Examples/Converters.cpp \
    Examples/ModuloModel.cpp \
    Examples/NumberDisplayDataModel.cpp \
    Examples/NumberSourceDataModel.cpp \
    Examples/NumberSourceDataUi.cpp \
    ThreadPull/EventThreadPull.cpp \
    Library/QDevIoDisplayModel.cpp \
    Library/QDevioDisplayModelUi.cpp \
    Library/XYSeriesIODevice.cpp

HEADERS += \
    NodeEditorInterface.h \
    NodeEditorPluginObject.h \
    Audio/AudioNodeQdevIoConnector.h \
    Audio/AudioSourceConfig.h \
    Audio/AudioSourceDataModel.h \
    Audio/AudioSourceDataModelUI.h \
    Audio/AudioWorker.h \
    Library/NodeDataModelToQIODeviceConnector.h \
    Examples/Converters.h \
    Examples/ModuloModel.h \
    Examples/NumberDisplayDataModel.h \
    Examples/NumberSourceDataModel.h \
    Examples/NumberSourceDataUi.h \
    Examples/Converters.h \
    Examples/DecimalData.h \
    Examples/IntegerData.h \
    Examples/ModuloModel.h \
    ThreadPull/EventThreadPull.h \
    Library/ComplexType.h \
    Library/QDevIoDisplayModel.h \
    Library/QDevioDisplayModelUi.h \
    Library/XYSeriesIODevice.h

FORMS += \
    Audio/AudioSourceConfig.ui \
    Audio/AudioSourceDataModelUI.ui \
    Examples/NumberSourceDataUi.ui \
    Library/QDevioDisplayModelUi.ui

RESOURCES += \
    Audio/node_editor.qrc

OTHER_FILES +=

win32 {
    # On Windows you can't mix release and debug libraries.
    # The designer is built in release mode. If you like to use it
    # you need a release version. For your own application development you
    # might need a debug version.
    # Enable debug_and_release + build_all if you want to build both.

#    CONFIG           += debug_and_release
#    CONFIG           += build_all
    CONFIG(debug, debug|release) {
        TARGET = $${TARGET}d
        LIBS        += -l$${FRAMEWORK_LIB_NAME}d
        LIBS        += -lnodesd
    }
    else{
        LIBS        += -l$${FRAMEWORK_LIB_NAME}
        LIBS        += -lnodes
    }
}else{
    LIBS        += -l$${FRAMEWORK_LIB_NAME}
    LIBS        += -lnodes
}

DISTFILES += \
    NodeEditorInterface.json
