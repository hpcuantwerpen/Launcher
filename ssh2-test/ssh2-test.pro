#-------------------------------------------------
#
# Project created by QtCreator 2015-07-14T12:15:21
#
#-------------------------------------------------

QT       += testlib

QT       -= gui

TARGET = tst_ssh2_test
CONFIG   += console
CONFIG   -= app_bundle

TEMPLATE = app


SOURCES += tst_ssh2_test.cpp
DEFINES += SRCDIR=\\\"$$PWD/\\\"

INCLUDEPATH += ../ssh2/
DEPENDPATH += $${INCLUDEPATH} # force rebuild if the headers change

unix {_SSH2_LIB = ../ssh2/libssh2.a}
LIBS += $${_SSH2_LIB}
unix {
    LIBS += -L/opt/local/lib/ -lssh2
}
PRE_TARGETDEPS += $${_SSH2_LIB}
