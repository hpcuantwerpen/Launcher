#-------------------------------------------------
#
# Project created by QtCreator 2015-07-13T07:47:22
#
#-------------------------------------------------

include(../common.pri)

QT       -= gui

TARGET = cfg
TEMPLATE = lib
CONFIG += staticlib

SOURCES += cfg.cpp

HEADERS += cfg.h

arg_path = ..
arg_lib  = toolbox
include(../depend_on_lib.pri)
