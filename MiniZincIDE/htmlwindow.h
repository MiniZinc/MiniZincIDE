#ifndef HTMLWINDOW_H
#define HTMLWINDOW_H

#include <QMainWindow>
#include <htmlpage.h>

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
private:
    Ui::HTMLWindow *ui;
    QVector<HTMLPage*> pages;
};

#endif // HTMLWINDOW_H
