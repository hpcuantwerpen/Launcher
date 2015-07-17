#-------------------------------------------------
#
# Project created by QtCreator 2015-07-14T12:15:21
#
#-------------------------------------------------

include(../common.pri)

QT       += testlib
QT       -= gui

TARGET = ssh2tools_test
CONFIG   += console
CONFIG   -= app_bundle

TEMPLATE = app


SOURCES += ssh2tools_test.cpp
DEFINES += SRCDIR=\\\"$$PWD/\\\"

arg_path = ..
arg_lib  = ssh2tools
include(../depend_on_lib.pri)
arg_lib  = toolbox
include(../depend_on_lib.pri)

#INCLUDEPATH += ../toolbox

include(../ssh2tools/libssh2.pri)
