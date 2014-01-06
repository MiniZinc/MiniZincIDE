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

#ifndef FINDDIALOG_H
#define FINDDIALOG_H

#include <QDialog>

namespace Ui {
class FindDialog;
}

class CodeEditor;

class FindDialog : public QDialog
{
    Q_OBJECT

public:
    explicit FindDialog(QWidget *parent = 0);
    ~FindDialog();

    void setEditor(CodeEditor* doc);
public slots:
    void on_b_next_clicked();

    void on_b_prev_clicked();

    void on_b_replacefind_clicked();

    void on_b_replace_clicked();

    void on_b_replaceall_clicked();

    void show();

private:
    Ui::FindDialog *ui;
    CodeEditor* codeEditor;

    void find(bool fwd);
};

#endif // FINDDIALOG_H
