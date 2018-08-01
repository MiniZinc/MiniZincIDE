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

#include "aboutdialog.h"
#include "ui_aboutdialog.h"

#ifndef MINIZINC_IDE_BUILD
#define MINIZINC_IDE_BUILD ""
#endif

AboutDialog::AboutDialog(QString version, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AboutDialog)
{
    ui->setupUi(this);
    QTextDocument* doc = ui->textBrowser->document();
    QTextCursor cursor = doc->find("$VERSION");
    cursor.insertText(version);
    cursor = doc->find("$BUILD");
    QString buildString(MINIZINC_IDE_BUILD);
    if (!buildString.isEmpty()) {
        buildString = "Build "+buildString;
    }
    cursor.insertText(buildString);
    ui->textBrowser->viewport()->setAutoFillBackground(false);
}

AboutDialog::~AboutDialog()
{
    delete ui;
}
