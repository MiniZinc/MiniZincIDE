#-------------------------------------------------
#
# Project created by QtCreator 2013-12-12T09:32:30
#
#-------------------------------------------------

QT       += core gui widgets

TARGET = MiniZincIDE
TEMPLATE = app

VERSION = 2.5.6
DEFINES += MINIZINC_IDE_VERSION=\\\"$$VERSION\\\"

bundled {
    DEFINES += MINIZINC_IDE_BUNDLED
}

CONFIG += c++11

macx {
    ICON = mznide.icns
    OBJECTIVE_SOURCES += macos_extras.mm
    QT += macextras
    LIBS += -framework Cocoa
    macx-xcode {
      QMAKE_INFO_PLIST = mznide-xcode.plist
    } else {
      QMAKE_INFO_PLIST = mznide-makefile.plist
    }
}

RC_ICONS = mznide.ico

CONFIG += embed_manifest_exe

SOURCES += main.cpp\
    codechecker.cpp \
    configwindow.cpp \
    elapsedtimer.cpp \
    extraparamdialog.cpp \
    ide.cpp \
    mainwindow.cpp \
    codeeditor.cpp \
    highlighter.cpp \
    fzndoc.cpp \
    process.cpp \
    projectbrowser.cpp \
    solverdialog.cpp \
    gotolinedialog.cpp \
    paramdialog.cpp \
    outputdockwidget.cpp \
    checkupdatedialog.cpp \
    project.cpp \
    moocsubmission.cpp \
    solverconfiguration.cpp \
    esclineedit.cpp

HEADERS  += mainwindow.h \
    codechecker.h \
    codeeditor.h \
    configwindow.h \
    elapsedtimer.h \
    exception.h \
    extraparamdialog.h \
    highlighter.h \
    fzndoc.h \
    ide.h \
    macos_extras.h \
    process.h \
    projectbrowser.h \
    solverdialog.h \
    gotolinedialog.h \
    paramdialog.h \
    outputdockwidget.h \
    checkupdatedialog.h \
    project.h \
    moocsubmission.h \
    solverconfiguration.h \
    esclineedit.h

FORMS    += \
    configwindow.ui \
    extraparamdialog.ui \
    mainwindow.ui \
    projectbrowser.ui \
    solverdialog.ui \
    gotolinedialog.ui \
    paramdialog.ui \
    checkupdatedialog.ui \
    moocsubmission.ui

RESOURCES += \
    minizincide.qrc

include($$PWD/../cp-profiler/cp-profiler.pri)
QT += network sql

target.path = $$PREFIX/bin
INSTALLS += target
