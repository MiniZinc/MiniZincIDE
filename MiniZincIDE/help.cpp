#include "help.h"
#include "ui_help.h"

#include <QCloseEvent>

Help::Help(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::Help)
{
    ui->setupUi(this);
}

Help::~Help()
{
    delete ui;
}

void Help::closeEvent(QCloseEvent* e)
{
    e->accept();
}
