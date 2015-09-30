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

SOURCES  += main.cpp        \
            mainwindow.cpp  \
            launcher.cpp \
            clusterinfo.cpp \
            dataconnector.cpp \
            job.cpp

HEADERS  += mainwindow.h   \
            launcher.h \
            clusterinfo.h \
            dataconnector.h \
            job.h

FORMS    += mainwindow.ui

arg_path = ..
arg_lib  = toolbox
include(../depend_on_lib.pri)
arg_lib  = cfg
include(../depend_on_lib.pri)
arg_lib  = jobscript
include(../depend_on_lib.pri)
arg_lib  = ssh2tools
include(../depend_on_lib.pri)

include(../ssh2tools/libssh2.pri)

DISTFILES += \
    DeployDocumentation.txt
