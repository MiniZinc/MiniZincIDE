#include "checkupdatedialog.h"
#include "ui_checkupdatedialog.h"

#include <QPushButton>

CheckUpdateDialog::CheckUpdateDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::CheckUpdateDialog)
{
    ui->setupUi(this);
}

CheckUpdateDialog::~CheckUpdateDialog()
{
    delete ui;
}

bool CheckUpdateDialog::sendStats(void) const {
    return ui->checkBox->isChecked();
}
