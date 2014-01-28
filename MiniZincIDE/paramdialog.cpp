#include "paramdialog.h"
#include "ui_paramdialog.h"

#include <QLabel>
#include <QLineEdit>
#include <QFormLayout>

ParamDialog::ParamDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ParamDialog)
{
    ui->setupUi(this);
}

QStringList ParamDialog::getParams(QStringList params)
{
    QVector<QLineEdit*> le;
    QFormLayout* formLayout = new QFormLayout;
    ui->frame->setLayout(formLayout);
    bool fillPrevious = previousParams == params;
    for (int i=0; i<params.size(); i++) {
        le.append(new QLineEdit(fillPrevious ? previousValues[i] : ""));
        formLayout->addRow(new QLabel(params[i]), le.back());
    }
    le[0]->setFocus();
    QStringList values;
    if (QDialog::exec()==QDialog::Accepted) {
        for (int i=0; i<le.size(); i++)
            values << le[i]->text();
        previousParams = params;
        previousValues = values;
    }
    delete formLayout;
    return values;
}

ParamDialog::~ParamDialog()
{
    delete ui;
}
