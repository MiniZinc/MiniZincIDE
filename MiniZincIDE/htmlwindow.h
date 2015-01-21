#ifndef HTMLWINDOW_H
#define HTMLWINDOW_H

#include <QMainWindow>

namespace Ui {
class HTMLWindow;
}

class MainWindow;

class HTMLWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit HTMLWindow(const QString& url, MainWindow* mw, QWidget *parent = 0);
    ~HTMLWindow();
    void addJson(const QString& json);
private:
    Ui::HTMLWindow *ui;
    QStringList json;
    bool loadFinished;
private slots:
    void pageLoadFinished(bool ok);
};

#endif // HTMLWINDOW_H
