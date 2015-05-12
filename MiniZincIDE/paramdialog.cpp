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
#include <QComboBox>
#include <QFormLayout>
#include <QFileInfo>
#include <QDebug>

ParamDialog::ParamDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ParamDialog)
{
    ui->setupUi(this);
}

void ParamDialog::getParams(QStringList params, const QStringList& dataFiles, QStringList &values, QString &dataFile)
{
    QVector<QLineEdit*> le;
    QFormLayout* formLayout = new QFormLayout;
    ui->frame->setLayout(formLayout);
    QComboBox* cb = NULL;
    if (dataFiles.size() > 0) {
        cb = new QComboBox();
        connect(cb, SIGNAL(currentIndexChanged(int)), this, SLOT(dataFileChanged(int)));
        cb->addItem("Select data file or input values");
        int selectItem = 0;
        for (int i=0; i<dataFiles.size(); i++) {
            QFileInfo fi(dataFiles[i]);
            cb->addItem(fi.fileName(),dataFiles[i]);
            if (previousDataFile==dataFiles[i])
                selectItem = i+1;
        }
        cb->setCurrentIndex(selectItem);
        formLayout->addRow(cb);
    }
    bool fillPrevious = previousParams == params;
    for (int i=0; i<params.size(); i++) {
        QLineEdit* e = new QLineEdit(fillPrevious ? previousValues[i] : "");
        if (cb && cb->currentIndex()!=0)
            e->setEnabled(false);
        le.append(e);
        formLayout->addRow(new QLabel(params[i]+" ="), le.back());
    }
    if (cb==NULL || cb->currentIndex()==0)
        le[0]->setFocus();
    if (QDialog::exec()==QDialog::Accepted) {
        if (cb && cb->currentIndex() != 0) {
            dataFile = cb->currentData().toString();
            previousDataFile = dataFile;
        } else {
            for (int i=0; i<le.size(); i++)
                values << le[i]->text();
            previousParams = params;
            previousValues = values;
        }
    }
    delete ui->frame;
    ui->frame = new QFrame;
    ui->frame->setFrameShape(QFrame::StyledPanel);
    ui->frame->setFrameShadow(QFrame::Raised);
    ui->verticalLayout->insertWidget(0, ui->frame);
}

void ParamDialog::dataFileChanged(int i)
{
    bool disable = (i>0);
    QFormLayout* l = static_cast<QFormLayout*>(ui->frame->layout());
    for (int i=1; i<l->rowCount(); i++) {
        QWidget* w = l->itemAt(i,QFormLayout::FieldRole)->widget();
        w->setDisabled(disable);
    }
}

ParamDialog::~ParamDialog()
{
    delete ui;
}
