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

#ifndef ADDOUTPUTDIALOG_H
#define ADDOUTPUTDIALOG_H

#include <QDialog>

namespace Ui {
class AddOutputDialog;
}

class AddOutputDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AddOutputDialog(QWidget *parent = 0);
    ~AddOutputDialog();

    QString getText();

private:
    Ui::AddOutputDialog *ui;
};

#endif // ADDOUTPUTDIALOG_H
