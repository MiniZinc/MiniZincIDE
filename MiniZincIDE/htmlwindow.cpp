#include "htmlwindow.h"
#include "ui_htmlwindow.h"
#include "htmlpage.h"

#include <QWebView>
#include <QMdiSubWindow>
#include <QDebug>
#include <QDockWidget>
#include <QCloseEvent>

HTMLWindow::HTMLWindow(const QVector<VisWindowSpec>& specs, MainWindow* mw, QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::HTMLWindow)
{
    ui->setupUi(this);

    for (int i=0; i<specs.size(); i++) {
        QWebView* wv = new QWebView;
        HTMLPage* p = new HTMLPage(mw,wv);
        pages.append(p);
        wv->setPage(p);
        loadQueue.append(QPair<QWebView*,QString>(wv,specs[i].url));
        QDockWidget* dw = new QDockWidget(this);
        dw->setFeatures(QDockWidget::DockWidgetMovable);
        dw->setWidget(wv);
        addDockWidget(specs[i].area,dw);
    }

    if (specs.size() > 0) {
        connect(loadQueue[0].first, SIGNAL(loadFinished(bool)), this, SLOT(loadFinished(bool)));
        QWebView* wv0 = loadQueue[0].first;
        QString url0 = loadQueue[0].second;
        wv0->load(QUrl::fromUserInput(url0));
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

void HTMLWindow::selectSolution(HTMLPage *source, int n)
{
    for (int i=0; i<pages.size(); i++) {
        if (pages[i] != source)
            pages[i]->showSolution(n);
    }
}

void HTMLWindow::finish(qint64 runtime)
{
    for (int i=0; i<pages.size(); i++) {
        pages[i]->finish(runtime);
    }
}

void HTMLWindow::loadFinished(bool)
{
    disconnect(loadQueue[0].first, SIGNAL(loadFinished(bool)), this, SLOT(loadFinished(bool)));
    loadQueue.pop_front();
    if (loadQueue.size() > 0) {
        connect(loadQueue[0].first, SIGNAL(loadFinished(bool)), this, SLOT(loadFinished(bool)));
        QWebView* wv0 = loadQueue[0].first;
        QString url0 = loadQueue[0].second;
        wv0->load(QUrl::fromUserInput(url0));
    }
}

void HTMLWindow::closeEvent(QCloseEvent * event)
{
    emit closeWindow();
    event->accept();
}
