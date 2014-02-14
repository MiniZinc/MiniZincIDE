#ifndef OUTPUTDOCKWIDGET_H
#define OUTPUTDOCKWIDGET_H

#include <QDockWidget>

class OutputDockWidget : public QDockWidget
{
    Q_OBJECT
public:
    explicit OutputDockWidget(QWidget *parent = 0)
        : QDockWidget(parent) {}

signals:

public slots:
protected:
    void closeEvent(QCloseEvent *event);
};

#endif // OUTPUTDOCKWIDGET_H
