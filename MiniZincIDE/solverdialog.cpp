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

#include "solverdialog.h"
#include "ui_solverdialog.h"
#include <QDebug>
#include <QMessageBox>
#include <QFileDialog>

SolverDialog::SolverDialog(QVector<Solver>& solvers0, const QString& def,
                           const QString& mznPath, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SolverDialog),
    solvers(solvers0),
    defaultSolver(0)
{
    ui->setupUi(this);

    ui->mznDistribPath->setText(mznPath);
    for (int i=solvers.size(); i--;) {
        ui->solvers_combo->insertItem(0,solvers[i].name,i);
    }
    ui->solvers_combo->setCurrentIndex(0);
    defaultSolver = ui->solvers_combo->findText(def);
    ui->solver_default->setChecked(0==defaultSolver);
    ui->solver_default->setEnabled(0!=defaultSolver);
}

SolverDialog::~SolverDialog()
{
    delete ui;
}

QString SolverDialog::mznPath()
{
    return ui->mznDistribPath->text();
}

QString SolverDialog::def()
{
    return ui->solvers_combo->itemText(defaultSolver);
}

void SolverDialog::on_solvers_combo_currentIndexChanged(int index)
{
    if (index<solvers.size()) {
        ui->name->setText(solvers[index].name);
        ui->executable->setText(solvers[index].executable);
        ui->detach->setChecked(solvers[index].detach);
        ui->mznpath->setText(solvers[index].mznlib);
        ui->backend->setText(solvers[index].backend);
        ui->solver_default->setChecked(index==defaultSolver);
        ui->solver_default->setEnabled(index!=defaultSolver);

        ui->updateButton->setText("Update");
        ui->deleteButton->setEnabled(true);
        ui->solverFrame->setEnabled(!solvers[index].builtin);
    } else {
        ui->name->setText("");
        ui->executable->setText("");
        ui->detach->setChecked(false);
        ui->mznpath->setText("");
        ui->backend->setText("");
        ui->solverFrame->setEnabled(true);
        ui->updateButton->setText("Add");
        ui->deleteButton->setEnabled(false);
    }
}

void SolverDialog::on_updateButton_clicked()
{
    int index = ui->solvers_combo->currentIndex();
    if (index==solvers.size()) {
        Solver s;
        solvers.append(s);
    }
    solvers[index].backend = ui->backend->text();
    solvers[index].executable = ui->executable->text();
    solvers[index].mznlib = ui->mznpath->text();
    solvers[index].name = ui->name->text();
    solvers[index].builtin = false;
    solvers[index].detach = ui->detach->isChecked();
    if (index==solvers.size()-1) {
        ui->solvers_combo->insertItem(index,ui->name->text(),index);
    }
    ui->solvers_combo->setCurrentIndex(index);
}

void SolverDialog::on_deleteButton_clicked()
{
    int index = ui->solvers_combo->currentIndex();
    if (QMessageBox::warning(this,"MiniZinc IDE","Delete solver "+solvers[index].name+"?",QMessageBox::Ok | QMessageBox::Cancel)
            == QMessageBox::Ok) {
        solvers.remove(index);
        if (ui->solver_default->isChecked())
            defaultSolver = 0;
        ui->solvers_combo->removeItem(index);
    }
}

void SolverDialog::on_mznpath_select_clicked()
{
    QFileDialog fd(this,"Select MiniZinc distribution path (bin directory)");
    QDir dir(ui->mznDistribPath->text());
    fd.setDirectory(dir);
    fd.setFileMode(QFileDialog::Directory);
    fd.setOption(QFileDialog::ShowDirsOnly, true);
    if (fd.exec()) {
        ui->mznDistribPath->setText(fd.selectedFiles().first());
    }
}

void SolverDialog::on_exec_select_clicked()
{
    QFileDialog fd(this,"Select solver executable");
    fd.selectFile(ui->executable->text());
    fd.setFileMode(QFileDialog::ExistingFile);
    if (fd.exec()) {
        ui->executable->setText(fd.selectedFiles().first());
    }
}

void SolverDialog::on_solver_default_stateChanged(int state)
{
    if (state==Qt::Checked) {
        defaultSolver = ui->solvers_combo->currentIndex();
        ui->solver_default->setEnabled(false);
    }
}
