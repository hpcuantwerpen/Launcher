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

SOURCES += jobscript.cpp \
    text.cpp \
    shellcommand.cpp \
    usercomment.cpp \
    shebang.cpp \
    launchercomment.cpp \
    pbsdirective.cpp

HEADERS += jobscript.h \
    text.h \
    shellcommand.h \
    usercomment.h \
    shebang.h \
    launchercomment.h \
    pbsdirective.h

arg_path = ..
arg_lib  = toolbox
include(../depend_on_lib.pri)
arg_lib  = cfg
include(../depend_on_lib.pri)

