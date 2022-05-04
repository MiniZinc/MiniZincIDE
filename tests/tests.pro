QT += testlib
CONFIG += testcase no_testcase_installs

DEFINES += MINIZINC_IDE_PATH=\\\"$$PWD\\\"
DEFINES += MINIZINC_IDE_TESTING

TEMPLATE = app

INCLUDEPATH += \
    $$PWD/.. \
    $$PWD/../MiniZincIDE

SOURCES += \
    testcpprofier.cpp \
    testide.cpp \
    testeditor.cpp \
    testmooc.cpp \
    testproject.cpp

HEADERS += \
    testide.h

include($$PWD/../MiniZincIDE/MiniZincIDE.pri)
