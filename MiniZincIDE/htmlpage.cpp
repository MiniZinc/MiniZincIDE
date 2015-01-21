#include "htmlpage.h"
#include "mainwindow.h"
#include "QDebug"

HTMLPage::HTMLPage(MainWindow* mw, QWidget *parent) :
    QWebPage(parent), _mw(mw)
{
}

void
HTMLPage::javaScriptConsoleMessage(const QString &message, int lineNumber, const QString &sourceID)
{
    _mw->addOutput("<div style='color:red;'>JavaScript message: source " +sourceID + ", line no. " + QString().number(lineNumber) + ": " + message + "</div>");
}
