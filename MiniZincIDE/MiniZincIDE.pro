#-------------------------------------------------
#
# Project created by QtCreator 2013-12-12T09:32:30
#
#-------------------------------------------------

QT       += core gui widgets websockets
TARGET = MiniZincIDE
TEMPLATE = app

VERSION = 2.6.1
DEFINES += MINIZINC_IDE_VERSION=\\\"$$VERSION\\\"

bundled {
    DEFINES += MINIZINC_IDE_BUNDLED
}

output_version {
    write_file($$OUT_PWD/version, VERSION)
}

CONFIG += c++11

macx {
    ICON = mznide.icns
    OBJECTIVE_SOURCES += \
    darkmodenotifier_macos.mm
    LIBS += -framework Cocoa
    macx-xcode {
      QMAKE_INFO_PLIST = mznide-xcode.plist
    } else {
      QMAKE_INFO_PLIST = mznide-makefile.plist
    }
}

win32 {
    LIBS += -ladvapi32
}

RC_ICONS = mznide.ico

CONFIG += embed_manifest_exe

SOURCES += main.cpp\
    codechecker.cpp \
    configwindow.cpp \
    darkmodenotifier.cpp \
    elapsedtimer.cpp \
    extraparamdialog.cpp \
    ide.cpp \
    ideutils.cpp \
    mainwindow.cpp \
    codeeditor.cpp \
    highlighter.cpp \
    fzndoc.cpp \
    outputwidget.cpp \
    preferencesdialog.cpp \
    process.cpp \
    profilecompilation.cpp \
    projectbrowser.cpp \
    server.cpp \
    gotolinedialog.cpp \
    paramdialog.cpp \
    outputdockwidget.cpp \
    checkupdatedialog.cpp \
    project.cpp \
    moocsubmission.cpp \
    esclineedit.cpp \
    solver.cpp

HEADERS  += mainwindow.h \
    codechecker.h \
    codeeditor.h \
    configwindow.h \
    darkmodenotifier.h \
    elapsedtimer.h \
    exception.h \
    extraparamdialog.h \
    highlighter.h \
    fzndoc.h \
    ide.h \
    ideutils.h \
    outputwidget.h \
    preferencesdialog.h \
    process.h \
    profilecompilation.h \
    projectbrowser.h \
    server.h \
    gotolinedialog.h \
    paramdialog.h \
    outputdockwidget.h \
    checkupdatedialog.h \
    project.h \
    moocsubmission.h \
    esclineedit.h \
    solver.h

FORMS    += \
    configwindow.ui \
    extraparamdialog.ui \
    mainwindow.ui \
    outputwidget.ui \
    preferencesdialog.ui \
    projectbrowser.ui \
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
