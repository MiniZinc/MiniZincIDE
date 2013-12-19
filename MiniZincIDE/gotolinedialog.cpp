#include "gotolinedialog.h"
#include "ui_gotolinedialog.h"

GoToLineDialog::GoToLineDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::GoToLineDialog)
{
    ui->setupUi(this);
    ui->line->setFocus();
}

GoToLineDialog::~GoToLineDialog()
{
    delete ui;
}

int GoToLineDialog::getLine(bool *ok) {
    return ui->line->text().toInt(ok);
}
