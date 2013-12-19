#ifndef GOTOLINEDIALOG_H
#define GOTOLINEDIALOG_H

#include <QDialog>

namespace Ui {
class GoToLineDialog;
}

class GoToLineDialog : public QDialog
{
    Q_OBJECT

public:
    explicit GoToLineDialog(QWidget *parent = 0);
    ~GoToLineDialog();

    int getLine(bool* ok);

private:
    Ui::GoToLineDialog *ui;
};

#endif // GOTOLINEDIALOG_H
