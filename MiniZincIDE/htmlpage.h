#ifndef HTMLPAGE_H
#define HTMLPAGE_H

class MainWindow;

#ifdef MINIZINC_IDE_HAVE_WEBENGINE

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
};

class HTMLPage : public QWebEnginePage
{
    Q_OBJECT
protected:
    MainWindow* _mw;
    QWebChannel* _webChannel;
    MiniZincIDEJS* _mznide;
    QStringList json;
    bool loadFinished;
    void runJs(QString js);
public:
    explicit HTMLPage(MainWindow* mw, QWidget *parent = 0);
    virtual void javaScriptConsoleMessage(JavaScriptConsoleMessageLevel level, const QString &message, int lineNumber, const QString &sourceID);
    void addSolution(const QString& json);
    void showSolution(int n);
    void finish(qint64 runtime);
public slots:
    void selectSolution(int n);

private slots:
    void pageLoadFinished(bool ok);
};

#else

#include <QWebPage>

class HTMLPage : public QWebPage
{
    Q_OBJECT
protected:
    MainWindow* _mw;
    QStringList json;
    bool loadFinished;
    void runJs(QString js);
public:
    explicit HTMLPage(MainWindow* mw, QWidget *parent = 0);
    virtual void javaScriptConsoleMessage(const QString &message, int lineNumber, const QString &sourceID);
    void addSolution(const QString& json);
    void showSolution(int n);
    void finish(qint64 runtime);
public slots:
    void selectSolution(int n);

private slots:
    void pageLoadFinished(bool ok);
    void jsCleared(void);
};

#endif

#endif // HTMLPAGE_H
