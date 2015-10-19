#-------------------------------------------------
#
# Project created by QtCreator 2015-07-14T10:24:34
#
#-------------------------------------------------
include(../common.pri)

QT       -= gui

TARGET = ssh2tools
TEMPLATE = lib
CONFIG += staticlib


SOURCES += ssh2tools.cpp

HEADERS += ssh2tools.h

arg_path = ..
arg_lib  = toolbox
include(../depend_on_lib.pri)

include(libssh2.pri)
