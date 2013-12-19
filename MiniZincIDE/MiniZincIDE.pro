#-------------------------------------------------
#
# Project created by QtCreator 2013-12-12T09:32:30
#
#-------------------------------------------------

QT       += core gui webkitwidgets

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = MiniZincIDE
TEMPLATE = app

ICON = mznide.icns

SOURCES += main.cpp\
        mainwindow.cpp \
    codeeditor.cpp \
    highlighter.cpp \
    webpage.cpp \
    fzndoc.cpp \
    aboutdialog.cpp \
    solverdialog.cpp \
    finddialog.cpp \
    findform.cpp \
    findreplacedialog.cpp \
    findreplaceform.cpp \
    gotolinedialog.cpp

HEADERS  += mainwindow.h \
    codeeditor.h \
    highlighter.h \
    webpage.h \
    fzndoc.h \
    aboutdialog.h \
    solverdialog.h \
    finddialog.h \
    findform.h \
    findreplace_global.h \
    findreplacedialog.h \
    findreplaceform.h \
    gotolinedialog.h

FORMS    += \
    mainwindow.ui \
    aboutdialog.ui \
    solverdialog.ui \
    findreplacedialog.ui \
    findreplaceform.ui \
    gotolinedialog.ui

RESOURCES += \
    minizincide.qrc
