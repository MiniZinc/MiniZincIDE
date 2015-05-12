#include "htmlpage.h"
#include "mainwindow.h"
#include "QDebug"
#include <QWebFrame>

HTMLPage::HTMLPage(MainWindow* mw, QWidget *parent) :
    QWebPage(parent), _mw(mw),
    loadFinished(false)
{
    connect(this, SIGNAL(loadFinished(bool)), this, SLOT(pageLoadFinished(bool)));
    connect(mainFrame(), SIGNAL(javaScriptWindowObjectCleared()), this, SLOT(jsCleared()));
}

void
HTMLPage::javaScriptConsoleMessage(const QString &message, int lineNumber, const QString &sourceID)
{
    _mw->addOutput("<div style='color:red;'>JavaScript message: source " +sourceID + ", line no. " + QString().number(lineNumber) + ": " + message + "</div><br>\n");
}

void
HTMLPage::jsCleared()
{
    mainFrame()->addToJavaScriptWindowObject("mznide", this);
}

void
HTMLPage::selectSolution(int n)
{
    _mw->selectJSONSolution(this,n);
}

void
HTMLPage::pageLoadFinished(bool ok)
{
    if (ok) {
        loadFinished = true;
        for (int i=0; i<json.size(); i++) {
            mainFrame()->evaluateJavaScript(json[i]);
        }
        json.clear();
    }
}

void
HTMLPage::addSolution(const QString &json0)
{
    QString j = json0;
    j.replace("'","\\'");
    j.replace("\"","\\\"");
    j.replace("\n"," ");
    if (loadFinished) {
        mainFrame()->evaluateJavaScript("addSolution('"+j+"')");
    } else {
        json.push_back("addSolution('"+j+"')");
    }
}

void
HTMLPage::finish(qint64 runtime)
{
    QString jscall = "if (typeof finish == 'function') { finish("+QString().number(runtime)+"); }";
    if (loadFinished) {
        mainFrame()->evaluateJavaScript(jscall);
    } else {
        json.push_back(jscall);
    }
}

void
HTMLPage::showSolution(int n)
{
    if (loadFinished) {
        mainFrame()->evaluateJavaScript("gotoSolution('"+QString().number(n)+"')");
    }
}
