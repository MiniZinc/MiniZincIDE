#include <QtTest>

#include "testide.h"

#include "ide.h"

#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QMessageBox>
#include <QTimer>

void TestIDE::testProject106Good()
{
    TestMocker mock(QString(MINIZINC_IDE_PATH) + "/data/project/project-106-good.mzp");
    MainWindow* w = mock.mw();
    QVERIFY(w->currentModelFile().endsWith("model1.mzn"));
    QCOMPARE(w->codeEditors().size(), 2);
    QCOMPARE(w->getProject().modelFiles().size(), 2);
    QCOMPARE(w->getProject().dataFiles().size(), 2);
    QCOMPARE(w->getProject().solverConfigurationFiles().size(), 2);
}

void TestIDE::testProject106Bad()
{
    QTimer::singleShot(200, this, [=] () {
        auto* message = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
        QVERIFY(message != nullptr);
        QVERIFY(message->text().startsWith("Failed to lookup solver invalid.solver@1.0.0"));
        message->accept();

        QTimer::singleShot(200, this, [=] () {
            auto* message = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
            QVERIFY(message != nullptr);
            QVERIFY(message->text().startsWith("The file file/does/not/exist.mzn could not be found"));
            message->accept();
        });
    });

    TestMocker mock(QString(MINIZINC_IDE_PATH) + "/data/project/project-106-bad.mzp");
    MainWindow* w = mock.mw();
    QVERIFY(w->currentModelFile().endsWith("model1.mzn"));
    QCOMPARE(w->codeEditors().size(), 2);
    QCOMPARE(w->getProject().modelFiles().size(), 2);
    QCOMPARE(w->getProject().dataFiles().size(), 2);
    QCOMPARE(w->getProject().solverConfigurationFiles().size(), 3);
}

void TestIDE::testProject105Good()
{
    TestMocker mock(QString(MINIZINC_IDE_PATH) + "/data/project/project-105-good.mzp");
    MainWindow* w = mock.mw();
    QVERIFY(w->currentModelFile().endsWith("model1.mzn"));
    QCOMPARE(w->codeEditors().size(), 2);
    QCOMPARE(w->getProject().modelFiles().size(), 2);
    QCOMPARE(w->getProject().dataFiles().size(), 2);

    QTimer::singleShot(200, this, [=] () {
        auto* message = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
        QVERIFY(message!= nullptr);
        QVERIFY(message->text().startsWith("There are modified solver configurations"));
        message->accept();
    });
}

void TestIDE::testProject105Bad()
{
    QTimer::singleShot(200, this, [=] () {
        auto* message = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
        QVERIFY(message != nullptr);
        QVERIFY(message->text().contains("Failed to lookup solver does.not.exist@0.1.0"));
        QVERIFY(message->text().contains("The file file/does/not/exist.mzn could not be found"));
        message->accept();
    });

    TestMocker mock(QString(MINIZINC_IDE_PATH) + "/data/project/project-105-bad.mzp");
    MainWindow* w = mock.mw();
    QVERIFY(w->currentModelFile().endsWith("model1.mzn"));
    QCOMPARE(w->codeEditors().size(), 2);
    QCOMPARE(w->getProject().modelFiles().size(), 2);
    QCOMPARE(w->getProject().dataFiles().size(), 2);

    QTimer::singleShot(200, this, [=] () {
        auto* message = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
        QVERIFY(message!= nullptr);
        QVERIFY(message->text().startsWith("There are modified solver configurations"));
        message->accept();
    });
}
