#-------------------------------------------------
#
# Project created by QtCreator 2015-07-14T13:47:10
#
#-------------------------------------------------

QT       -= gui

TARGET = jobscript
TEMPLATE = lib
CONFIG += staticlib
CONFIG += c++11

SOURCES += jobscript.cpp

HEADERS += jobscript.h
unix {
    target.path = /usr/lib
    INSTALLS += target
}
