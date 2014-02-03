/*
 *  Author:
 *     Guido Tack <guido.tack@monash.edu>
 *
 *  Copyright:
 *     NICTA 2013
 */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
        formLayout->addRow(new QLabel(params[i]+" ="), le.back());
    }
    le[0]->setFocus();
    QStringList values;
    if (QDialog::exec()==QDialog::Accepted) {
        for (int i=0; i<le.size(); i++)
            values << le[i]->text();
        previousParams = params;
        previousValues = values;
    }
    for (int i=0; i<params.size(); i++) {
        QWidget* w = formLayout->itemAt(i,QFormLayout::LabelRole)->widget();
        formLayout->removeWidget(w);
        delete w;
        w = formLayout->itemAt(i,QFormLayout::FieldRole)->widget();
        formLayout->removeWidget(w);
        delete w;
    }
    delete formLayout;
    return values;
}

ParamDialog::~ParamDialog()
{
    delete ui;
}
