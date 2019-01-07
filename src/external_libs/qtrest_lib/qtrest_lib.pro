#-------------------------------------------------
#
# Project created by QtCreator 2019-01-03T11:50:55
#
#-------------------------------------------------
include(../../include/paths_cfg.pri)
include(./qtrest/com_github_kafeg_qtrest.pri)
INCLUDEPATH += $$PWD
DEPENDPATH  += $$PWD
OBJECTS_DIR  = $$PWD/Build
MOC_DIR      = $$PWD/Build
RCC_DIR      = $$PWD/Build
UI_DIR       = $$PWD/Build
DESTDIR=$${EXT_LIBS_DIR}
QT += core gui qml
QT += quickcontrols2

TARGET = qtrest_lib
TEMPLATE = lib

DEFINES += QTREST_LIB_LIBRARY

a.path = $${EXT_LIBS_INCLUDES_DIR}/qtrest_lib
a.files += ./*.h
INSTALLS += a


# The following define makes your compiler emit warnings if you use
# any feature of Qt which has been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
        qtrest_lib.cpp \
    api/Exchange/ExchangeApi.cpp \
    api/Exchange/model/ExchangeModel.cpp

HEADERS += \
        qtrest_lib.h \
        qtrest_lib_global.h \ 
    api/Exchange/ExchangeApi.h \
    api/Exchange/model/ExchangeModel.h

unix {
  #  target.path = /usr/lib
  #  INSTALLS += target
}
