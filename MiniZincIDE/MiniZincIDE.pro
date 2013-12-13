#-------------------------------------------------
#
# Project created by QtCreator 2013-12-12T09:32:30
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = MiniZincIDE
TEMPLATE = app


SOURCES += main.cpp\
        mainwindow.cpp \
    codeeditor.cpp \
    highlighter.cpp \
    rundialog.cpp

HEADERS  += mainwindow.h \
    codeeditor.h \
    highlighter.h \
    rundialog.h

FORMS    += \
    mainwindow.ui \
    rundialog.ui
