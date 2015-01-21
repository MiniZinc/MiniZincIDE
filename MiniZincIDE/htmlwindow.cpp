#include "htmlwindow.h"
#include "ui_htmlwindow.h"
#include "htmlpage.h"

#include "QWebFrame"

#include <QDebug>

HTMLWindow::HTMLWindow(const QString& url, MainWindow* mw, QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::HTMLWindow),
    loadFinished(false)
{
    ui->setupUi(this);
    ui->webView->setPage(new HTMLPage(mw));
    ui->webView->load(url);
    connect(ui->webView, SIGNAL(loadFinished(bool)), this, SLOT(pageLoadFinished(bool)));
}

HTMLWindow::~HTMLWindow()
{
    delete ui;
}

void
HTMLWindow::pageLoadFinished(bool ok)
{
    if (ok) {
        loadFinished = true;
        for (int i=0; i<json.size(); i++) {
            ui->webView->page()->mainFrame()->evaluateJavaScript(json[i]);
        }
        json.clear();
    }
}

void
HTMLWindow::addJson(const QString &json0)
{
    QString j = json0;
    j.replace("'","\\'");
    j.replace("\"","\\\"");
    j.replace("\n"," ");
    if (loadFinished) {
        ui->webView->page()->mainFrame()->evaluateJavaScript("addSolution('"+j+"')");
    } else {
        json.push_back("addSolution('"+j+"')");
    }
}
