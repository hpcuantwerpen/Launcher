#-------------------------------------------------
#
# Project created by QtCreator 2015-07-01T12:18:37
#
#-------------------------------------------------
include(../common.pri)

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = Launcher
TEMPLATE = app

macx {
	ICON = ../Launcher.icns
}

CONFIG   -= console

QMAKE_CXXFLAGS += -Wno-format-security

SOURCES  += main.cpp        \
            mainwindow.cpp  \
            launcher.cpp \
            clusterinfo.cpp \
            dataconnector.cpp \
            job.cpp \
    messagebox.cpp \
    walltime.cpp

HEADERS  += mainwindow.h   \
            version.h \
            launcher.h \
            clusterinfo.h \
            dataconnector.h \
            job.h \
    messagebox.h \
    walltime.h

FORMS    += mainwindow.ui

DEFINES  += BETA_RELEASE

# The order below matters!  undefined references may appear if wrong
# e.g in libtoolbox.a (referenced in libjobscript.a) if toolbox comes before jobscript.
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




