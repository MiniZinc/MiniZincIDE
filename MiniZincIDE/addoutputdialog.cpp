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

#include "addoutputdialog.h"
#include "ui_addoutputdialog.h"

AddOutputDialog::AddOutputDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AddOutputDialog)
{
    ui->setupUi(this);
    ui->addoutput->setFocus();
}

AddOutputDialog::~AddOutputDialog()
{
    delete ui;
}

QString AddOutputDialog::getText() {
    return ui->addoutput->text();
}
