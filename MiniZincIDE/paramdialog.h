#ifndef PARAMDIALOG_H
#define PARAMDIALOG_H

#include <QDialog>
#include <QLineEdit>

namespace Ui {
class ParamDialog;
}

class ParamDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ParamDialog(QWidget *parent = 0);
    ~ParamDialog();
    QStringList getParams(QStringList params);
private:
    Ui::ParamDialog *ui;
    QStringList previousParams;
    QStringList previousValues;
};

#endif // PARAMDIALOG_H
