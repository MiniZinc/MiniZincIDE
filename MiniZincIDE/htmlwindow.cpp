#include "htmlwindow.h"
#include "ui_htmlwindow.h"
#include "htmlpage.h"

#include <QWebView>
#include <QDebug>

HTMLWindow::HTMLWindow(const QStringList& url, MainWindow* mw, QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::HTMLWindow)
{
    ui->setupUi(this);

    for (int i=0; i<url.size(); i++) {
        QWebView* wv = new QWebView;
        HTMLPage* p = new HTMLPage(mw);
        pages.append(p);
        wv->setPage(p);
        wv->load(url[i]);
        ui->mdiArea->addSubWindow(wv);
    }

}

HTMLWindow::~HTMLWindow()
{
    delete ui;
}

void HTMLWindow::addSolution(int nVis, const QString &json)
{
    pages[nVis]->addSolution(json);
}
