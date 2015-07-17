#-------------------------------------------------
#
# Project created by QtCreator 2015-07-16T22:28:33
#
#-------------------------------------------------

include(../common.pri)

QT       += testlib
QT       -= gui

TARGET = toolbox_test

CONFIG   += console
CONFIG   -= app_bundle
CONFIG += link_prl

TEMPLATE = app

SOURCES += toolbox_test.cpp
DEFINES += SRCDIR=\\\"$$PWD/\\\"

arg_path = ..
arg_lib  = toolbox
include(../depend_on_lib.pri)
