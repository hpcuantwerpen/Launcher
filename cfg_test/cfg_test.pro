#-------------------------------------------------
#
# Project created by QtCreator 2015-07-13T07:49:17
#
#-------------------------------------------------

include(../common.pri)

QT       += testlib
QT       -= gui

TARGET = cfg_test
CONFIG += console
CONFIG -= app_bundle

TEMPLATE = app

SOURCES += cfg_test.cpp
DEFINES += SRCDIR=\\\"$$PWD/\\\"

arg_path = ..
arg_lib  = cfg
include(../depend_on_lib.pri)
