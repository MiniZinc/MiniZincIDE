#-------------------------------------------------
#
# Project created by QtCreator 2013-12-12T09:32:30
#
#-------------------------------------------------

QT       += core gui webkitwidgets

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = MiniZincIDE
TEMPLATE = app

VERSION = 0.9.2
DEFINES += MINIZINC_IDE_VERSION=\\\"$$VERSION\\\"

macx {
    ICON = mznide.icns
    QMAKE_INFO_PLIST = mznide.plist
}

RC_ICONS = mznide.ico

CONFIG += embed_manifest_exe

SOURCES += main.cpp\
        mainwindow.cpp \
    codeeditor.cpp \
    highlighter.cpp \
    fzndoc.cpp \
    aboutdialog.cpp \
    solverdialog.cpp \
    gotolinedialog.cpp \
    help.cpp \
    finddialog.cpp \
    paramdialog.cpp

HEADERS  += mainwindow.h \
    codeeditor.h \
    highlighter.h \
    fzndoc.h \
    aboutdialog.h \
    solverdialog.h \
    gotolinedialog.h \
    help.h \
    finddialog.h \
    paramdialog.h

FORMS    += \
    mainwindow.ui \
    aboutdialog.ui \
    solverdialog.ui \
    gotolinedialog.ui \
    help.ui \
    finddialog.ui \
    paramdialog.ui

RESOURCES += \
    minizincide.qrc
