#-------------------------------------------------
#
# Project created by QtCreator 2012-06-21T13:34:25
#
#-------------------------------------------------
include(../../include/paths_cfg.pri)
include(qtrest/com_github_kafeg_qtrest.pri)
DESTDIR      = $${PLUGINS_DEST_DIR}
QT          += core gui network qml
QT += webenginewidgets
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

TEMPLATE     = lib
CONFIG      += plugin
CONFIG      += create_prl
CONFIG      += link_prl

DEFINES     += BUILD_AVAILABLE_PLUGIN

TARGET   = QtCoinTraderPlugin

SOURCES += \
    QtCoinTraderInterface.cpp \
    QtCoinTraderPluginObject.cpp \
    QtCoinTraderWindow.cpp \
    RestApiTester.cpp \
    RequestForm.cpp

HEADERS += \
    QtCoinTraderInterface.h \
    QtCoinTraderPluginObject.h \
    QtCoinTraderWindow.h \
    RestApiTester.h \
    RequestForm.h

FORMS += \
    mainwindow.ui \
    restapitester.ui \
    RequestForm.ui

RESOURCES += \
    QtCoinTrader.qrc

OTHER_FILES += \
    QtCoinTraderInterface.json

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
    }
    else{
        LIBS        += -l$${FRAMEWORK_LIB_NAME}
    }
}else{
    LIBS        += -l$${FRAMEWORK_LIB_NAME}
}

win32:CONFIG(release, debug|release): LIBS += -L$$OUT_PWD/RestApi/release/ -lRestApi
else:win32:CONFIG(debug, debug|release): LIBS += -L$$OUT_PWD/RestApi/debug/ -lRestApi
else:unix: LIBS += -L$$OUT_PWD/RestApi/ -lRestApi

INCLUDEPATH += $$PWD/RestApi
DEPENDPATH += $$PWD/RestApi
