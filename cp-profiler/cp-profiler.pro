TEMPLATE = app

TARGET = cp-profiler

QT += widgets network sql

CONFIG += c++11

include(cp-profiler.pri)
SOURCES += $$PWD/src/main_cpprofiler.cpp
