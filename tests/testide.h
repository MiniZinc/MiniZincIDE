#include <QObject>
#include <QSettings>
#include <QDate>

#include "mainwindow.h"

class TestIDE : public QObject
{
    Q_OBJECT

private slots:
    void testStartupPlayground();
    void testIndent();
    void testIndent_data();
    void testFindReplace();
    void testFindReplace_data();

    void testProject106Good();
    void testProject106Bad();
    void testProject105Good();
    void testProject105Bad();

    void testMoocProject();
    void testMoocTerms();
    void testMoocSubmission();

    void testCPProfiler();

    void testDiff();
    void testDiffApply();
    void testDiffApply_data();
    void testHistory();
};

class TestMocker {
public:
    template <typename ...T>
    TestMocker(T&& ...args): _mw(new MainWindow(std::forward<T>(args)...)) {
        TestMocker::resetSettings();
        _mw->show();
    }

    ~TestMocker() {
        _mw->close();
        _mw->deleteLater();
    }

    MainWindow* mw() const { return _mw; }

    static void resetSettings();
private:
    MainWindow* _mw;
};
