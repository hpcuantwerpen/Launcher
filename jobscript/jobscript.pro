#-------------------------------------------------
#
# Project created by QtCreator 2015-07-14T13:47:10
#
#-------------------------------------------------

include(../common.pri)

QT       -= gui

TARGET = jobscript
TEMPLATE = lib
CONFIG += staticlib

SOURCES += jobscript.cpp

HEADERS += jobscript.h

arg_path = ..
arg_lib  = toolbox
include(../depend_on_lib.pri)
