#include <QtTest>

#include "testide.h"

#include "mainwindow.h"
#include "ui_mainwindow.h"

void TestIDE::testStartupPlayground()
{
    TestMocker mock;
    MainWindow* w = mock.mw();
    QString text = w->curEditor->document()->toPlainText();
    QCOMPARE(text, QString("% Use this editor as a MiniZinc scratch book\n"));
}

void TestIDE::testIndent()
{
    QFETCH(QString, initial);
    QFETCH(int, shift_amount);
    QFETCH(int, start);
    QFETCH(int, end);
    QFETCH(QString, expected);

    TestMocker mock;
    MainWindow* w = mock.mw();
    CodeEditor* e = w->curEditor;
    QTextDocument* doc = e->document();
    doc->setPlainText(initial);
    QTextCursor cursor = e->textCursor();
    cursor.setPosition(start, QTextCursor::MoveAnchor);
    cursor.setPosition(end, QTextCursor::KeepAnchor);
    e->setTextCursor(cursor);
    while (shift_amount < 0) {
        w->on_actionShift_left_triggered();
        shift_amount++;
    }
    while (shift_amount > 0) {
        w->on_actionShift_right_triggered();
        shift_amount--;
    }
    QCOMPARE(doc->toPlainText(), expected);
    doc->setModified(false);
}

void TestIDE::testIndent_data() {
    QString initial1 = "Line 1\n"
                       "  Line 2\n"
                       "    Line 3\n";
    QString initial2 = "Line 1\n"
                       "  Line 2\n"
                       "    Line 3";
    QString initial3 = "Line 1\n"
                       "Line 2\n"
                       "Line 3\n";
    QString initial4 = "Line 1\n"
                       "Line 2\n"
                       "Line 3";

    QTest::addColumn<int>("shift_amount");
    QTest::addColumn<QString>("initial");
    QTest::addColumn<int>("start");
    QTest::addColumn<int>("end");
    QTest::addColumn<QString>("expected");

    QTest::newRow("shift all left")
            << -1
            << initial1
            << 0
            << 26
            << "Line 1\n"
               "Line 2\n"
               "  Line 3\n";

    QTest::newRow("shift first line left")
            << -1
            << initial1
            << 0
            << 6
            << "Line 1\n"
               "  Line 2\n"
               "    Line 3\n";

    QTest::newRow("shift last line (back) left")
            << -1
            << initial1
            << 26
            << 16
            << "Line 1\n"
               "  Line 2\n"
               "  Line 3\n";

    QTest::newRow("shift last two lines (back, no trail) left")
            << -1
            << initial2
            << 26
            << 7
            << "Line 1\n"
               "Line 2\n"
               "  Line 3";

    QTest::newRow("shift empty line left")
            << -1
            << initial1
            << 27
            << 27
            << initial1;

    QTest::newRow("shift partial line left")
            << -1
            << initial1
            << 9
            << 11
            << "Line 1\n"
               "Line 2\n"
               "    Line 3\n";

    QTest::newRow("shift 2nd line, no selection left")
            << -1
            << initial1
            << 10
            << 10
            << "Line 1\n"
               "Line 2\n"
               "    Line 3\n";

    QTest::newRow("shift all left twice")
            << -2
            << initial1
            << 0
            << 26
            << "Line 1\n"
               "Line 2\n"
               "Line 3\n";

    QTest::newRow("shift all left three times")
            << -3
            << initial1
            << 0
            << 26
            << "Line 1\n"
               "Line 2\n"
               "Line 3\n";

    QTest::newRow("shift all right")
            << 1
            << initial3
            << 0
            << 20
            << "  Line 1\n"
               "  Line 2\n"
               "  Line 3\n";

    QTest::newRow("shift first line right")
            << 1
            << initial3
            << 0
            << 7
            << "  Line 1\n"
               "Line 2\n"
               "Line 3\n";

    QTest::newRow("shift last line (back)")
            << 1
            << initial3
            << 20
            << 14
            << "Line 1\n"
               "Line 2\n"
               "  Line 3\n";

    QTest::newRow("shift last two lines (back, no trail) right")
            << 1
            << initial4
            << 20
            << 7
            << "Line 1\n"
               "  Line 2\n"
               "  Line 3";

    QTest::newRow("shift empty line right")
            << 1
            << initial3
            << 21
            << 21
            << "Line 1\n"
               "Line 2\n"
               "Line 3\n  ";

    QTest::newRow("shift partial line selection right")
            << 1
            << initial3
            << 9
            << 11
            << "Line 1\n"
               "  Line 2\n"
               "Line 3\n";


    QTest::newRow("shift 2nd line, no selection")
            << 1
            << initial3
            << 10
            << 10
            << "Line 1\n"
               "  Line 2\n"
               "Line 3\n";
}

enum FindReplaceAction {
    NEXT,
    PREVIOUS,
    REPLACE,
    REPLACE_AND_FIND,
    REPLACE_ALL
};

Q_DECLARE_METATYPE(FindReplaceAction);

void TestIDE::testFindReplace() {
    QFETCH(QString, initial);
    QFETCH(bool, regex);
    QFETCH(bool, ignore_case);
    QFETCH(bool, wrap_around);
    QFETCH(QString, find);
    QFETCH(QString, replace);
    QFETCH(QVector<FindReplaceAction>, actions);
    QFETCH(QString, expected);

    TestMocker mock;
    MainWindow* w = mock.mw();
    CodeEditor* e = w->curEditor;
    QTextDocument* doc = e->document();
    doc->setPlainText(initial);
    w->curEditor->moveCursor(QTextCursor::Start);
    w->ui->find->setText(find);
    w->ui->replace->setText(replace);
    w->ui->check_re->setChecked(regex);
    w->ui->check_case->setChecked(ignore_case);
    w->ui->check_wrap->setChecked(wrap_around);

    QWidget* buttons[] = {
        w->ui->b_next,
        w->ui->b_prev,
        w->ui->b_replace,
        w->ui->b_replacefind,
        w->ui->b_replaceall
    };

    for (int i = 0; i < actions.size(); ++i) {
        QTest::mouseClick(buttons[actions.at(i)], Qt::LeftButton);
    }

    QCOMPARE(doc->toPlainText(), expected);
    doc->setModified(false);
}

void TestIDE::testFindReplace_data() {
    QString initial = "Foo\n"
                      "Foo bar\n"
                      "bAr fOo";

    QTest::addColumn<QString>("initial");
    QTest::addColumn<bool>("regex");
    QTest::addColumn<bool>("ignore_case");
    QTest::addColumn<bool>("wrap_around");
    QTest::addColumn<QString>("find");
    QTest::addColumn<QString>("replace");
    QTest::addColumn<QVector<FindReplaceAction> >("actions");
    QTest::addColumn<QString>("expected");

    QVector<FindReplaceAction> basicActions;
    basicActions
            << FindReplaceAction::NEXT
            << FindReplaceAction::REPLACE;

    QTest::addRow("find replace basic")
            << initial
            << false
            << false
            << false
            << "Foo"
            << "baz"
            << basicActions
            << "baz\n"
               "Foo bar\n"
               "bAr fOo";

    QVector<FindReplaceAction> basicActions2;
    basicActions2
            << FindReplaceAction::NEXT
            << FindReplaceAction::REPLACE
            << FindReplaceAction::NEXT
            << FindReplaceAction::REPLACE;

    QTest::addRow("find replace basic twice")
            << initial
            << false
            << false
            << false
            << "Foo"
            << "baz"
            << basicActions2
            << "baz\n"
               "baz bar\n"
               "bAr fOo";
}
