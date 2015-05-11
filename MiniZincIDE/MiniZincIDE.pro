#-------------------------------------------------
#
# Project created by QtCreator 2013-12-12T09:32:30
#
#-------------------------------------------------

QT       += core gui webkitwidgets

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = MiniZincIDE
TEMPLATE = app

VERSION = 0.9.5
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
    project.cpp \

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
    rtfexporter.h \

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


# ***** STANDALONE_GIST PART *****

greaterThan(QT_MAJOR_VERSION, 4): QT += printsupport

CONFIG += c++11

SOURCES += \
    ../StandaloneGist/gistmainwindow.cpp \
    ../StandaloneGist/globalhelper.cpp \
    ../StandaloneGist/heap.cpp \
    ../StandaloneGist/nodewidget.cpp \
    ../StandaloneGist/drawingcursor.cpp \
    ../StandaloneGist/treecanvas.cpp \
    ../StandaloneGist/visualnode.cpp \
    ../StandaloneGist/nodestats.cpp \
    ../StandaloneGist/preferences.cpp \
    ../StandaloneGist/qtgist.cpp \
    ../StandaloneGist/spacenode.cpp \
    ../StandaloneGist/node.cpp \
    ../StandaloneGist/data.cpp \
    ../StandaloneGist/base_tree_dialog.cpp \
    ../StandaloneGist/solver_tree_dialog.cpp \
    ../StandaloneGist/cmp_tree_dialog.cpp \
    ../StandaloneGist/receiverthread.cpp \
    ../StandaloneGist/treebuilder.cpp \
    ../StandaloneGist/readingQueue.cpp \
    ../StandaloneGist/treecomparison.cpp \
    ../StandaloneGist/pixelview.cpp \


HEADERS  += \
    ../StandaloneGist/gistmainwindow.h \
    ../StandaloneGist/globalhelper.hh \
    ../StandaloneGist/qtgist.hh \
    ../StandaloneGist/treecanvas.hh \
    ../StandaloneGist/visualnode.hh \
    ../StandaloneGist/spacenode.hh \
    ../StandaloneGist/node.hh \
    ../StandaloneGist/node.hpp \
    ../StandaloneGist/spacenode.hpp \
    ../StandaloneGist/visualnode.hpp \
    ../StandaloneGist/heap.hpp \
    ../StandaloneGist/nodestats.hh \
    ../StandaloneGist/preferences.hh \
    ../StandaloneGist/nodewidget.hh \
    ../StandaloneGist/drawingcursor.hh \
    ../StandaloneGist/drawingcursor.hpp \
    ../StandaloneGist/nodecursor.hh \
    ../StandaloneGist/nodecursor.hpp \
    ../StandaloneGist/layoutcursor.hh \
    ../StandaloneGist/layoutcursor.hpp \
    ../StandaloneGist/nodevisitor.hh \
    ../StandaloneGist/nodevisitor.hpp \
    ../StandaloneGist/zoomToFitIcon.hpp \
    ../StandaloneGist/data.hh \
    ../StandaloneGist/base_tree_dialog.hh \
    ../StandaloneGist/solver_tree_dialog.hh \
    ../StandaloneGist/cmp_tree_dialog.hh \
    ../StandaloneGist/receiverthread.hh \
    ../StandaloneGist/treebuilder.hh \
    ../StandaloneGist/readingQueue.hh \
    ../StandaloneGist/treecomparison.hh \
    ../StandaloneGist/pixelview.hh

win32:CONFIG(release, debug|release): LIBS += -L$$PWD/../../../../../../../usr/local/lib/release/ -lzmq
else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/../../../../../../../usr/local/lib/debug/ -lzmq
else:unix: LIBS += -L$$PWD/../../../../../../usr/local/lib/ -lzmq -ldl

INCLUDEPATH += $$PWD/../../../../../../../../usr/local/include
DEPENDPATH += $$PWD/../../../../../../../../usr/local/include

win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += $$PWD/../../../../../../../usr/local/lib/release/libzmq.a
else:win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += $$PWD/../../../../../../../usr/local/lib/debug/libzmq.a
else:win32:!win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += $$PWD/../../../../../../../usr/local/lib/release/zmq.lib
else:win32:!win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += $$PWD/../../../../../../../usr/local/lib/debug/zmq.lib
else:unix: PRE_TARGETDEPS += $$PWD/../../../../../../../usr/local/lib/libzmq.a
