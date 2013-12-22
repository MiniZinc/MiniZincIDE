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
