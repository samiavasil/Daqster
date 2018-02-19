#-------------------------------------------------
#
# Project created by QtCreator 2012-06-29T11:52:27
#
#-------------------------------------------------
QT       += core gui widgets
#QT += network
OBJECTS_DIR  = $$PWD/Build
MOC_DIR      = $$PWD/Build
RCC_DIR      = $$PWD/Build
UI_DIR       = $$PWD/Build


TARGET   = Daqster
TEMPLATE = app
DESTDIR = ../../../bin
INCLUDEPATH += ../../frame_work/base/src/include
INCLUDEPATH += ../../plugins/QtCoinTrader/RestApi
FRAMEWORK_LIB_NAME = frame_work

SOURCES += main.cpp\
    AppToolbar.cpp \
    ApplicationsManager.cpp
HEADERS  += \
    AppToolbar.h \
    ApplicationsManager.h \
    main.h


LIBS += -L../../../bin/libs  -L../../../bin/extlibs
#-lRestApi
CONFIG+= c++11

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

RESOURCES += icons.qrc

FORMS += \
    mainwindow.ui

DISTFILES += \
    StyleFile
