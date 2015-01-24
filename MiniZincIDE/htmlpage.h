#ifndef HTMLPAGE_H
#define HTMLPAGE_H

#include <QWebPage>

class MainWindow;

class HTMLPage : public QWebPage
{
    Q_OBJECT
protected:
    MainWindow* _mw;
    QStringList json;
    bool loadFinished;
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

#endif // HTMLPAGE_H
