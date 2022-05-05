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

    qRegisterMetaType<cpprofiler::Execution*>();
    QSignalSpy spy(w->conductor, &cpprofiler::Conductor::executionStart);
    QSignalSpy spy2(w, &MainWindow::finished);
    w->on_actionProfile_search_triggered();
    QVERIFY(spy.wait());
    auto args = spy.takeFirst();
    auto* ex = args[0].value<cpprofiler::Execution*>();
    QVERIFY(ex != nullptr);
    QVERIFY(QTest::qWaitFor([=] () {
        return ex->tree().nodeCount() == 5;
    }, 30000));
    QVERIFY(spy2.count() > 0 || spy2.wait());
}
