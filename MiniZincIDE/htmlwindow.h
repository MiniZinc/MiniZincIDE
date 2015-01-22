#ifndef HTMLWINDOW_H
#define HTMLWINDOW_H

#include <QMainWindow>
#include <QWebView>
#include "htmlpage.h"

namespace Ui {
class HTMLWindow;
}

class MainWindow;

class HTMLWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit HTMLWindow(const QStringList& url, MainWindow* mw, QWidget *parent = 0);
    ~HTMLWindow();

    void addSolution(int nVis, const QString& json);
    void selectSolution(HTMLPage* source, int n);
private:
    Ui::HTMLWindow *ui;
    QVector<HTMLPage*> pages;
    QVector<QPair<QWebView*,QString> > loadQueue;
private slots:
    void loadFinished(bool);
};

#endif // HTMLWINDOW_H
