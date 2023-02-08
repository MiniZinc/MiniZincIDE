#include <QtTest>
#include <QSignalSpy>

#include "testide.h"

#include "history.h"

void TestIDE::testDiff()
{
    QString origText =
            "foo\n"
            "bar\n"
            "qux\n"
            "baz";
    QString newText =
            "foo\n"
            "hello\n"
            "bar\n"
            "baz\n"
            "world";
    FileDiff diff(origText, newText);
    auto result = diff.diff();
    QCOMPARE(result.size(), 3);
    QCOMPARE(result[0].kind, FileDiff::EntryType::Insertion);
    QCOMPARE(result[0].line, 2);
    QCOMPARE(result[0].text, "hello");
    QCOMPARE(result[1].kind, FileDiff::EntryType::Deletion);
    QCOMPARE(result[1].line, 3);
    QCOMPARE(result[1].text, "qux");
    QCOMPARE(result[2].kind, FileDiff::EntryType::Insertion);
    QCOMPARE(result[2].line, 5);
    QCOMPARE(result[2].text, "world");
}

void TestIDE::testDiffApply()
{
    QFETCH(QString, origText);
    QFETCH(QString, newText);
    FileDiff diff(origText, newText);
    auto result = diff.diff();
    auto applied = FileDiff::apply(origText, result);
    QCOMPARE(applied, newText);
}

void TestIDE::testDiffApply_data() {
    QTest::addColumn<QString>("origText");
    QTest::addColumn<QString>("newText");
    QTest::newRow("test 1") << "a\nb\nc\nd" << "a\nc\nd\ne";
    QTest::newRow("test 2") << "a\nb\nc\nd" << "b\nc\nd";
    QTest::newRow("test 3") << "a\nb\nc\nd\nb\na\nc" << "b\na\nc";
}

void TestIDE::testHistory()
{
    History history("foo");
    QSignalSpy spy(&history, &History::historyChanged);
    history.addFile("test.mzn", "a\nb\nc\n");
    history.updateFileContents("test.mzn", "a\nc\nd\n");
    history.updateFileContents("test.mzn", "f\na\nc\ne\n");
    history.commit();
    QCOMPARE(spy.count(), 3);
}
