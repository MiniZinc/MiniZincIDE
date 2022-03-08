TARGET = MiniZincIDE
TEMPLATE = app

INCLUDEPATH += $$PWD/../

SOURCES += main.cpp

include($$PWD/MiniZincIDE.pri)

target.path = $$PREFIX/bin
INSTALLS += target
