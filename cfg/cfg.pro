#-------------------------------------------------
#
# Project created by QtCreator 2015-07-13T07:47:22
#
#-------------------------------------------------

QT       -= gui

TARGET = cfg
TEMPLATE = lib
CONFIG += staticlib
CONFIG += c++11

SOURCES += cfg.cpp

HEADERS += cfg.h
unix {
    target.path = /usr/lib
    INSTALLS += target
}
