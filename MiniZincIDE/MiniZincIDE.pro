#-------------------------------------------------
#
# Project created by QtCreator 2013-12-12T09:32:30
#
#-------------------------------------------------

QT       += core gui webkitwidgets

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = MiniZincIDE
TEMPLATE = app

VERSION = 0.9.6
DEFINES += MINIZINC_IDE_VERSION=\\\"$$VERSION\\\"

macx {
    ICON = mznide.icns
    QMAKE_INFO_PLIST = mznide.plist
    OBJECTIVE_SOURCES += rtfexporter.mm
    QT += macextras
    LIBS += -framework Cocoa
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
    paramdialog.cpp \
    outputdockwidget.cpp \
    checkupdatedialog.cpp \
    project.cpp

HEADERS  += mainwindow.h \
    codeeditor.h \
    highlighter.h \
    fzndoc.h \
    aboutdialog.h \
    solverdialog.h \
    gotolinedialog.h \
    help.h \
    finddialog.h \
    paramdialog.h \
    outputdockwidget.h \
    checkupdatedialog.h \
    project.h \
    rtfexporter.h

FORMS    += \
    mainwindow.ui \
    aboutdialog.ui \
    solverdialog.ui \
    gotolinedialog.ui \
    help.ui \
    finddialog.ui \
    paramdialog.ui \
    checkupdatedialog.ui

RESOURCES += \
    minizincide.qrc
