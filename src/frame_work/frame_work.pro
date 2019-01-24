#-------------------------------------------------
#
# Project created by QtCreator 2012-06-29T11:55:23
#
#-------------------------------------------------
DESTDIR = ../../bin/libs
#CONFIG += debug_and_release
TARGET = frame_work
CONFIG+= c++11
#win32|mac:!wince*:!win32-msvc:!macx-xcode:CONFIG += debug_and_release build_all
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
    }
}

TEMPLATE = lib
INCLUDEPATH += $$PWD/base/src
INCLUDEPATH += $$PWD/base/src/include/
INCLUDEPATH += $$PWD/../include/extlibs
DEFINES += FRAME_WORK_LIBRARY
OBJECTS_DIR  = $$PWD/Build
MOC_DIR      = $$PWD/Build
RCC_DIR      = $$PWD/Build
UI_DIR       = $$PWD/Build


greaterThan(QT_MAJOR_VERSION, 4) {
    QT += widgets
}

SOURCES += \
    base/src/QBasePluginObject.cpp \
    base/src/PluginDescription.cpp \
    base/src/QPluginListView.cpp \
    base/src/QPluginManager.cpp \
    base/src/QPluginManagerGui.cpp \
    base/src/PluginFilter.cpp \
    base/src/QPluginLoaderExt.cpp \
    base/src/PluginDetails.cpp \
    base/src/QPluginInterface.cpp

HEADERS += \
    base/src/include/QBasePluginObject.h \
    base/src/include/QPluginManager.h \
    base/src/include/PluginDescription.h \
    base/src/include/build_cfg.h \
    base/src/include/debug.h \
    base/src/global.h \
    base/src/QPluginListView.h \
    base/src/QPluginManagerGui.h \
    base/src/PluginFilter.h \
    base/src/Singleton.h \
    base/src/plugin_global.h \
    base/src/QPluginLoaderExt.h \
    base/src/PluginDetails.h \
    base/src/include/QPluginInterface.h

symbian {
    MMP_RULES += EXPORTUNFROZEN
    TARGET.UID3 = 0xEC538F15
    TARGET.CAPABILITY = 
    TARGET.EPOCALLOWDLLDATA = 1
    addFiles.sources = frame_work.dll
    addFiles.path = !:/sys/bin
    DEPLOYMENT += addFiles
}

unix:!symbian {
    maemo5 {
        target.path = /opt/usr/lib
    } else {
        target.path = /usr/lib
    }
    INSTALLS += target
}

FORMS += \
    base/src/pluginlistview.ui \
    base/src/pluginmanagergui.ui \
    base/src/plugindetails.ui

RESOURCES += \
    framework_resources.qrc
