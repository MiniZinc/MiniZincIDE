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

#ifndef GOTOLINEDIALOG_H
#define GOTOLINEDIALOG_H

#include <QDialog>

namespace Ui {
class GoToLineDialog;
}

class GoToLineDialog : public QDialog
{
    Q_OBJECT

public:
    explicit GoToLineDialog(QWidget *parent = 0);
    ~GoToLineDialog();

    int getLine(bool* ok);

private:
    Ui::GoToLineDialog *ui;
};

#endif // GOTOLINEDIALOG_H
