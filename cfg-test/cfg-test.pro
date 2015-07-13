#-------------------------------------------------
#
# Project created by QtCreator 2015-07-13T07:49:17
#
#-------------------------------------------------

QT       += testlib

QT       -= gui

TARGET = tst_cfg_test
CONFIG   += console
CONFIG   -= app_bundle

TEMPLATE = app


SOURCES += tst_cfg_test.cpp
DEFINES += SRCDIR=\\\"$$PWD/\\\"

INCLUDEPATH += ../cfg/
DEPENDPATH += $${INCLUDEPATH} # force rebuild if the headers change

# link against cfglib
unix {_CFG_LIB = ../cfg/libcfg.a}
LIBS += $${_CFG_LIB}
PRE_TARGETDEPS += $${_CFG_LIB}
