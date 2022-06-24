QT       += core gui widgets websockets

VERSION = 2.7.0
DEFINES += MINIZINC_IDE_VERSION=\\\"$$VERSION\\\"

bundled {
    DEFINES += MINIZINC_IDE_BUNDLED
}

output_version {
    write_file($$OUT_PWD/version, VERSION)
}

CONFIG += c++11

macx {
    ICON = $$PWD/mznide.icns
    OBJECTIVE_SOURCES += \
        $$PWD/darkmodenotifier_macos.mm
    LIBS += -framework Cocoa
    macx-xcode {
      QMAKE_INFO_PLIST = $$PWD/mznide-xcode.plist
    } else {
      QMAKE_INFO_PLIST = $$PWD/mznide-makefile.plist
    }
}

win32 {
    LIBS += -ladvapi32
}

RC_ICONS = $$PWD/mznide.ico

CONFIG += embed_manifest_exe

SOURCES += \
    $$PWD/codechecker.cpp \
    $$PWD/configwindow.cpp \
    $$PWD/darkmodenotifier.cpp \
    $$PWD/elapsedtimer.cpp \
    $$PWD/extraparamdialog.cpp \
    $$PWD/ide.cpp \
    $$PWD/ideutils.cpp \
    $$PWD/mainwindow.cpp \
    $$PWD/codeeditor.cpp \
    $$PWD/highlighter.cpp \
    $$PWD/fzndoc.cpp \
    $$PWD/outputwidget.cpp \
    $$PWD/preferencesdialog.cpp \
    $$PWD/process.cpp \
    $$PWD/profilecompilation.cpp \
    $$PWD/projectbrowser.cpp \
    $$PWD/server.cpp \
    $$PWD/gotolinedialog.cpp \
    $$PWD/paramdialog.cpp \
    $$PWD/outputdockwidget.cpp \
    $$PWD/checkupdatedialog.cpp \
    $$PWD/project.cpp \
    $$PWD/moocsubmission.cpp \
    $$PWD/esclineedit.cpp \
    $$PWD/solver.cpp

HEADERS += \
    $$PWD/mainwindow.h \
    $$PWD/codechecker.h \
    $$PWD/codeeditor.h \
    $$PWD/configwindow.h \
    $$PWD/darkmodenotifier.h \
    $$PWD/elapsedtimer.h \
    $$PWD/exception.h \
    $$PWD/extraparamdialog.h \
    $$PWD/highlighter.h \
    $$PWD/fzndoc.h \
    $$PWD/ide.h \
    $$PWD/ideutils.h \
    $$PWD/outputwidget.h \
    $$PWD/preferencesdialog.h \
    $$PWD/process.h \
    $$PWD/profilecompilation.h \
    $$PWD/projectbrowser.h \
    $$PWD/server.h \
    $$PWD/gotolinedialog.h \
    $$PWD/paramdialog.h \
    $$PWD/outputdockwidget.h \
    $$PWD/checkupdatedialog.h \
    $$PWD/project.h \
    $$PWD/moocsubmission.h \
    $$PWD/esclineedit.h \
    $$PWD/solver.h

FORMS += \
    $$PWD/configwindow.ui \
    $$PWD/extraparamdialog.ui \
    $$PWD/mainwindow.ui \
    $$PWD/outputwidget.ui \
    $$PWD/preferencesdialog.ui \
    $$PWD/projectbrowser.ui \
    $$PWD/gotolinedialog.ui \
    $$PWD/paramdialog.ui \
    $$PWD/checkupdatedialog.ui \
    $$PWD/moocsubmission.ui

RESOURCES += \
    $$PWD/minizincide.qrc

include($$PWD/../cp-profiler/cp-profiler.pri)
QT += network sql
