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

#include "finddialog.h"
#include "ui_finddialog.h"
#include "codeeditor.h"
#include <QDebug>

FindDialog::FindDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::FindDialog),
    codeEditor(NULL)
{
    ui->setupUi(this);
}

FindDialog::~FindDialog()
{
    delete ui;
}

void FindDialog::show()
{
    ui->not_found->setText("");
    QDialog::show();
}

void FindDialog::setEditor(CodeEditor* e)
{
    codeEditor = e;
}

void FindDialog::find(bool fwd)
{
    const QString& toFind = ui->find->text();
    QTextDocument::FindFlags flags;
    if (!fwd)
        flags |= QTextDocument::FindBackward;
    bool ignoreCase = ui->check_case->isChecked();
    if (!ignoreCase)
        flags |= QTextDocument::FindCaseSensitively;
    bool wrap = ui->check_wrap->isChecked();

    QTextCursor cursor(codeEditor->textCursor());
    int hasWrapped = wrap ? 0 : 1;
    while (hasWrapped < 2) {
        if (ui->check_re->isChecked()) {
            QRegExp re(toFind, ignoreCase ? Qt::CaseInsensitive : Qt::CaseSensitive);
            if (!re.isValid()) {
                ui->not_found->setText("invalid");
                return;
            }
            cursor = codeEditor->document()->find(re,cursor,flags);
        } else {
            cursor = codeEditor->document()->find(toFind,cursor,flags);
        }
        if (cursor.isNull()) {
            hasWrapped++;
            cursor = codeEditor->textCursor();
            if (fwd) {
                cursor.setPosition(0);
            } else {
                cursor.movePosition(QTextCursor::End);
            }
        } else {
            ui->not_found->setText("");
            codeEditor->setTextCursor(cursor);
            break;
        }
    }
    if (hasWrapped==2) {
        ui->not_found->setText("not found");
    }
}

void FindDialog::on_b_next_clicked()
{
    find(true);
}

void FindDialog::on_b_prev_clicked()
{
    find(false);
}

void FindDialog::on_b_replacefind_clicked()
{
    QTextCursor cursor = codeEditor->textCursor();
    if (cursor.hasSelection()) {
        cursor.insertText(ui->replace->text());
    }
    find(true);
}

void FindDialog::on_b_replace_clicked()
{
    QTextCursor cursor = codeEditor->textCursor();
    if (cursor.hasSelection()) {
        cursor.insertText(ui->replace->text());
    }
}

void FindDialog::on_b_replaceall_clicked()
{
    int counter = 0;
    QTextCursor cursor = codeEditor->textCursor();
    if (!cursor.hasSelection()) {
        find(true);
        cursor = codeEditor->textCursor();
    }
    cursor.beginEditBlock();
    while (cursor.hasSelection()) {
        counter++;
        cursor.insertText(ui->replace->text());
        find(true);
        cursor = codeEditor->textCursor();
    }
    cursor.endEditBlock();
    if (counter > 0) {
        ui->not_found->setText(QString().number(counter)+" replaced");
    }
}
