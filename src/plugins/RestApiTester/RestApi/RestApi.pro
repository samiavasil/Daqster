#-------------------------------------------------
#
# Project created by QtCreator 2018-02-14T17:19:27
#
#-------------------------------------------------

QT -= gui
QT += network
include(../../../include/paths_cfg.pri)
CONFIG += c++11
DESTDIR      = $${LIBS_DIR}
TARGET = RestApi
TEMPLATE = lib

DEFINES += RESTAPI_LIBRARY

SOURCES += RestApi.cpp

HEADERS += RestApi.h\
        restapi_global.h

unix {
    target.path = /usr/lib
    INSTALLS += target
}
