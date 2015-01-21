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

private slots:
    void pageLoadFinished(bool ok);

};

#endif // HTMLPAGE_H
