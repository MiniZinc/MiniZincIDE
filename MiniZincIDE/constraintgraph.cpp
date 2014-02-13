#include <QtWebKitWidgets>

#include "constraintgraph.h"
#include "ui_constraintgraph.h"

#include "webpage.h"
#include "fzndoc.h"

ConstraintGraph::ConstraintGraph(QString fzn, QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::ConstraintGraph), _fzn(fzn)
{
    ui->setupUi(this);
    WebPage* page = new WebPage();
    ui->webView->setPage(page);
    QString url = "qrc:/ConstraintGraph/index.html";
    connect(ui->webView, SIGNAL(loadFinished(bool)), this, SLOT(webview_loaded(bool)));
    ui->webView->load(url);
    show();
}

ConstraintGraph::~ConstraintGraph()
{
    delete ui;
}

void ConstraintGraph::webview_loaded(bool ok) {
    if (ok){
        FznDoc fzndoc;
        fzndoc.setstr(_fzn);
        ui->webView->page()->mainFrame()->addToJavaScriptWindowObject("fznfile", &fzndoc);
        QString code = "start_s(fznfile)";
        ui->webView->page()->mainFrame()->evaluateJavaScript(code);
    } else {
        qDebug() << "not ok";
    }
}
