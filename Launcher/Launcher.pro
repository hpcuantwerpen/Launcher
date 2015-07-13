#-------------------------------------------------
#
# Project created by QtCreator 2015-07-01T12:18:37
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = Launcher
TEMPLATE = app


SOURCES  += main.cpp        \
            mainwindow.cpp  \
            launcher.cpp

HEADERS  += mainwindow.h   \
            launcher.h

FORMS    += mainwindow.ui

QMAKE_CXXFLAGS += -std=c++0x
