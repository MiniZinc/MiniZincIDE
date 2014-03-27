#ifndef CHECKUPDATEDIALOG_H
#define CHECKUPDATEDIALOG_H

#include <QDialog>

namespace Ui {
class CheckUpdateDialog;
}

class CheckUpdateDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CheckUpdateDialog(QWidget *parent = 0);
    ~CheckUpdateDialog();
    bool sendStats(void) const;
private:
    Ui::CheckUpdateDialog *ui;
};

#endif // CHECKUPDATEDIALOG_H
