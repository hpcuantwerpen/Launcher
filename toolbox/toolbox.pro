#-------------------------------------------------
#
# Project created by QtCreator 2015-07-16T15:17:13
#
#-------------------------------------------------

include(../common.pri)

QT       -= gui

TARGET = toolbox

TEMPLATE = lib
CONFIG += staticlib
#CONFIG += create_prl

HEADERS += orderedmap.h     \
           warn_.h          \
           throw_.h         \
           toolbox.h

SOURCES += orderedmap.cpp   \
           warn_.cpp

