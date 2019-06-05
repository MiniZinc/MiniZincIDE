#ifndef HTMLPAGE_H
#define HTMLPAGE_H

class MainWindow;

#include <QWebEnginePage>
#include <QWebChannel>

class HTMLPage;

class MiniZincIDEJS : public QObject {
    Q_OBJECT
protected:
    HTMLPage* _htmlPage;
public:
    MiniZincIDEJS(HTMLPage* p);
public slots:
    void selectSolution(int n);
    void solve(const QString& data);
};

class HTMLPage : public QWebEnginePage
{
    Q_OBJECT
protected:
    MainWindow* _mw;
    QWebChannel* _webChannel;
    MiniZincIDEJS* _mznide;
    int _htmlWindowIdentifier;
    QStringList json;
    bool loadFinished;
    void runJs(QString js);
public:
    explicit HTMLPage(MainWindow* mw, int htmlWindowIdentifier, QWidget *parent = 0);
    virtual void javaScriptConsoleMessage(JavaScriptConsoleMessageLevel level, const QString &message, int lineNumber, const QString &sourceID);
    void addSolution(const QString& json);
    void initJSON(const QString& json);
    void showSolution(int n);
    void finish(qint64 runtime);
public slots:
    void selectSolution(int n);
    void solve(const QString& data);

private slots:
    void pageLoadFinished(bool ok);
};

#endif // HTMLPAGE_H
