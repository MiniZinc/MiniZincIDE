#ifndef CONSTRAINTGRAPH_H
#define CONSTRAINTGRAPH_H

#include <QMainWindow>

namespace Ui {
class ConstraintGraph;
}

class ConstraintGraph : public QMainWindow
{
    Q_OBJECT

public:
    explicit ConstraintGraph(QString fzn, QWidget *parent = 0);
    ~ConstraintGraph();

private:
    Ui::ConstraintGraph *ui;
    QString _fzn;

private slots:
    void webview_loaded(bool);

};

#endif // CONSTRAINTGRAPH_H
