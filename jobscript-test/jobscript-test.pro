#-------------------------------------------------
#
# Project created by QtCreator 2015-07-14T13:51:21
#
#-------------------------------------------------

QT       += testlib

QT       -= gui

TARGET = tst_jobscript_test
CONFIG   += console
CONFIG   -= app_bundle
CONFIG += c++11

TEMPLATE = app


SOURCES += tst_jobscript_test.cpp
DEFINES += SRCDIR=\\\"$$PWD/\\\"

INCLUDEPATH += ../jobscript/
DEPENDPATH += $${INCLUDEPATH} # force rebuild if the headers change

# link against jobscript lib
unix {_jobscript_LIB = ../jobscript/libjobscript.a}
LIBS += $${_jobscript_LIB}
PRE_TARGETDEPS += $${_jobscript_LIB}
