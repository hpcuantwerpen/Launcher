#-------------------------------------------------
#
# Project created by QtCreator 2015-07-14T13:51:21
#
#-------------------------------------------------

include(../common.pri)

QT       += testlib
QT       -= gui

TARGET = jobscript_test
CONFIG += console
CONFIG -= app_bundle

TEMPLATE = app

SOURCES += jobscript_test.cpp
DEFINES += SRCDIR=\\\"$$PWD/\\\"

arg_path = ..
arg_lib  = jobscript
include(../depend_on_lib.pri)
arg_lib  = toolbox
include(../depend_on_lib.pri)
arg_lib  = cfg
include(../depend_on_lib.pri)

#INCLUDEPATH += ../toolbox
