#include "rundialog.h"
#include "ui_rundialog.h"

RunDialog::RunDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::RunDialog)
{
    ui->setupUi(this);
}

RunDialog::~RunDialog()
{
    delete ui;
}
