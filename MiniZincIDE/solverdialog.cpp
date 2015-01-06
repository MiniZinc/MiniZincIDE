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
#include "mainwindow.h"
#include <QDebug>
#include <QMessageBox>
#include <QFileDialog>
#include <QSettings>
#include <QProcess>

#include <QtGlobal>
#ifdef Q_OS_WIN
#define pathSep ";"
#define fileDialogSuffix "/"
#define MZNOS "win"
#else
#define pathSep ":"
#ifdef Q_OS_MAC
#define fileDialogSuffix "/*"
#define MZNOS "mac"
#else
#define fileDialogSuffix "/"
#define MZNOS "linux"
#endif
#endif

SolverDialog::SolverDialog(QVector<Solver>& solvers0, const QString& def,
                           bool openAsAddNew,
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
    QSettings settings;
    settings.beginGroup("ide");
    ui->check_updates->setChecked(settings.value("checkforupdates",false).toBool());
    ui->send_stats->setChecked(settings.value("sendstats",false).toBool());
    ui->send_stats->setEnabled(ui->check_updates->isChecked());
    settings.endGroup();
    if (openAsAddNew)
        ui->solvers_combo->setCurrentIndex(ui->solvers_combo->count()-1);
    editingFinished(false);
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
        on_mznDistribPath_editingFinished();
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

void SolverDialog::on_check_updates_stateChanged(int checkstate)
{
    QSettings settings;
    settings.beginGroup("ide");
    settings.setValue("checkforupdates", checkstate==Qt::Checked);
    settings.endGroup();
    ui->send_stats->setEnabled(checkstate==Qt::Checked);
}

void SolverDialog::on_send_stats_stateChanged(int checkstate)
{
    QSettings settings;
    settings.beginGroup("ide");
    settings.setValue("sendstats", checkstate==Qt::Checked);
    settings.endGroup();
}

void SolverDialog::checkMzn2fznExecutable(const QString& mznDistribPath,
                                          QString& mzn2fzn_executable,
                                          QString& mzn2fzn_version_string)
{
    MznProcess p;
    QStringList args;
    args << "--version";
    mzn2fzn_executable = "";
    mzn2fzn_version_string = "";
    p.start("mzn2fzn", args, mznDistribPath);
    if (!p.waitForStarted() || !p.waitForFinished()) {
        p.start("mzn2fzn.bat", args, mznDistribPath);
        if (!p.waitForStarted() || !p.waitForFinished()) {
            return;
        } else {
            mzn2fzn_executable = "mzn2fzn.bat";
        }
    } else {
        mzn2fzn_executable = "mzn2fzn";
    }
    mzn2fzn_version_string = p.readAllStandardOutput()+p.readAllStandardError();
}

void SolverDialog::editingFinished(bool showError)
{
    QString mzn2fzn_executable;
    QString mzn2fzn_version;
    checkMzn2fznExecutable(ui->mznDistribPath->text(),mzn2fzn_executable,mzn2fzn_version);
    if (mzn2fzn_executable.isEmpty()) {
        if (showError) {
            QMessageBox::warning(this,"MiniZinc IDE","Could not find the mzn2fzn executable.",
                                 QMessageBox::Ok);
        }
        if (mzn2fzn_version.isEmpty()) {
            ui->mzn2fzn_version->setText("None");
        } else {
            ui->mzn2fzn_version->setText("None. Error message:\n"+mzn2fzn_version);
        }
    } else {
        ui->mzn2fzn_version->setText(mzn2fzn_version);
    }
}

void SolverDialog::on_mznDistribPath_editingFinished()
{
    editingFinished(true);
}

void MznProcess::start(const QString &program, const QStringList &arguments, const QString &path)
{
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    QString curPath = env.value("PATH");
    QString addPath = IDE::instance()->appDir();
    if (!path.isEmpty())
        addPath = path + pathSep + addPath;
    env.insert("PATH", addPath + pathSep + curPath);
    setProcessEnvironment(env);
#ifdef Q_OS_WIN
    _putenv_s("PATH", (addPath + pathSep + curPath).toStdString().c_str());
#else
    setenv("PATH", (addPath + pathSep + curPath).toStdString().c_str(), 1);
#endif
    QProcess::start(program,arguments);
#ifdef Q_OS_WIN
    _putenv_s("PATH", curPath.toStdString().c_str());
#else
    setenv("PATH", curPath.toStdString().c_str(), 1);
#endif
}
