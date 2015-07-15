#-------------------------------------------------
#
# Project created by QtCreator 2015-07-14T10:24:34
#
#-------------------------------------------------

QT       -= gui

TARGET = ssh2
TEMPLATE = lib
CONFIG += staticlib
CONFIG += c++11

SOURCES += ssh2.cpp

HEADERS += ssh2.h
unix {
    LIBS += -L/opt/local/lib/ -lssh2
    INCLUDEPATH += /opt/local/include
    target.path = /usr/lib
    INSTALLS += target
}
