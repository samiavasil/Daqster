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

TARGET   = NodeEditorPlugin

SOURCES += \
    NodeEditorPluginObject.cpp \
    NumberDisplayDataModel.cpp \
    NumberSourceDataModel.cpp \
    ModuloModel.cpp \
    Converters.cpp \
    NodeEditorInterface.cpp \
    NumbeSourceDataUi.cpp \
    XYSeriesIODevice.cpp \
    AudioSourceWidget.cpp \
    AudioSourceDataModel.cpp \
    AudioNodeQdevIoConnector.cpp \
    QDevIoDisplayModel.cpp \
    QDevioDisplayModelUi.cpp

HEADERS += \
    NodeEditorInterface.h \
    NodeEditorPluginObject.h \
    IntegerData.h \
    ModuloModel.h \
    NumberDisplayDataModel.h \
    NumberSourceDataModel.h \
    DecimalData.h \
    Converters.h \
    NumericType.h \
    ComplexType.h \
    XYSeriesIODevice.h \
    AudioSourceWidget.h \
    AudioSourceDataModel.h \
    NumberSourceDataUi.h \
    AudioNodeQdevIoConnector.h \
    QDevIoDisplayModel.h \
    QDevioDisplayModelUi.h

FORMS += \
    AudioSourceWidget.ui \
    NumberSourceDataUi.ui \
    QDevioDisplayModelUi.ui

RESOURCES += \
    node_editor.qrc

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
