#include <QtTest>

#include "testide.h"

#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "../cp-profiler/src/cpprofiler/execution.hh"

#include <QSignalSpy>

void TestIDE::testCPProfiler()
{
    TestMocker mock;
    MainWindow* w = mock.mw();
    CodeEditor* e = w->curEditor;
    QTextDocument* doc = e->document();
    doc->setPlainText("var 1..3: x;"
                      "solve :: int_search([x], input_order, indomain_min) maximize x;");
    doc->setModified(false);
    w->on_actionShow_search_profiler_triggered();

    connect(w->conductor, &cpprofiler::Conductor::executionFinish, [=] (cpprofiler::Execution* ex) {
        QVERIFY(ex != nullptr);
        QCOMPARE(ex->tree().nodeCount(), 5);
    });

    QSignalSpy spy(w->conductor, &cpprofiler::Conductor::executionFinish);
    w->on_actionProfile_search_triggered();
    spy.wait();
}
