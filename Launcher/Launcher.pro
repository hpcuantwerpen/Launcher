#-------------------------------------------------
#
# Project created by QtCreator 2015-07-01T12:18:37
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = Launcher
TEMPLATE = app

CONFIG   += c++11
CONFIG   -= console

QMAKE_CXXFLAGS += -Wno-format-security

SOURCES  += main.cpp        \
            mainwindow.cpp  \
            launcher.cpp \
            clusterinfo.cpp \
            dataconnector.cpp \
            job.cpp

HEADERS  += mainwindow.h   \
            version.h \
            launcher.h \
            clusterinfo.h \
            dataconnector.h \
            job.h

FORMS    += mainwindow.ui

# The order below matters! In cygwin we get undefined references to functions
# in libtoolbox.a which are referenced in libjobscript.a if toolbox comes
# before jobscript.
arg_path = ..
arg_lib  = cfg
include(../depend_on_lib.pri)
arg_lib  = jobscript
include(../depend_on_lib.pri)
arg_lib  = toolbox
include(../depend_on_lib.pri)
arg_lib  = ssh2tools
include(../depend_on_lib.pri)

include(../ssh2tools/libssh2.pri)




