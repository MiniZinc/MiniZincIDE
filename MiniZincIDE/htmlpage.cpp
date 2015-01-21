#include "htmlpage.h"
#include "mainwindow.h"
#include "QDebug"
#include <QWebFrame>

HTMLPage::HTMLPage(MainWindow* mw, QWidget *parent) :
    QWebPage(parent), _mw(mw),
    loadFinished(false)
{
    connect(this, SIGNAL(loadFinished(bool)), this, SLOT(pageLoadFinished(bool)));
}

void
HTMLPage::javaScriptConsoleMessage(const QString &message, int lineNumber, const QString &sourceID)
{
    _mw->addOutput("<div style='color:red;'>JavaScript message: source " +sourceID + ", line no. " + QString().number(lineNumber) + ": " + message + "</div>");
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
