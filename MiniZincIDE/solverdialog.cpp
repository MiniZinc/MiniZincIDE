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
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

#ifndef Q_OS_WIN
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <iostream>
#endif

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

SolverDialog::SolverDialog(QVector<Solver>& solvers0,
                           QString& userSolverConfigDir0,
                           QString& mznStdlibDir0,
                           bool openAsAddNew,
                           const QString& mznPath, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SolverDialog),
    solvers(solvers0), userSolverConfigDir(userSolverConfigDir0), mznStdlibDir(mznStdlibDir0)
{
    ui->setupUi(this);

    ui->mznDistribPath->setText(mznPath);
    for (int i=solvers.size(); i--;) {
        ui->solvers_combo->insertItem(0,solvers[i].name+" "+solvers[i].version,i);
    }
    ui->solvers_combo->setCurrentIndex(0);
    QSettings settings;
    settings.beginGroup("ide");
    ui->check_updates->setChecked(settings.value("checkforupdates21",false).toBool());
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

void SolverDialog::on_solvers_combo_currentIndexChanged(int index)
{
    if (index<solvers.size()) {
        ui->name->setText(solvers[index].name);
        ui->solverId->setText(solvers[index].id);
        ui->version->setText(solvers[index].version);
        ui->executable->setText(solvers[index].executable);
        ui->detach->setChecked(solvers[index].isGUIApplication);
        ui->needs_mzn2fzn->setChecked(!solvers[index].supportsMzn);
        ui->mznpath->setText(solvers[index].mznlib);
        ui->updateButton->setText("Update");
        ui->deleteButton->setEnabled(true);
        ui->solverFrame->setEnabled(!solvers[index].configFile.isEmpty());

        ui->has_stdflag_a->setChecked(solvers[index].stdFlags.contains("-a"));
        ui->has_stdflag_p->setChecked(solvers[index].stdFlags.contains("-p"));
        ui->has_stdflag_r->setChecked(solvers[index].stdFlags.contains("-r"));
        ui->has_stdflag_n->setChecked(solvers[index].stdFlags.contains("-n"));
        ui->has_stdflag_s->setChecked(solvers[index].stdFlags.contains("-s"));
        ui->has_stdflag_f->setChecked(solvers[index].stdFlags.contains("-f"));
    } else {
        ui->name->setText("");
        ui->solverId->setText("");
        ui->version->setText("");
        ui->executable->setText("");
        ui->detach->setChecked(false);
        ui->needs_mzn2fzn->setChecked(true);
        ui->mznpath->setText("");
        ui->solverFrame->setEnabled(true);
        ui->updateButton->setText("Add");
        ui->deleteButton->setEnabled(false);
        ui->has_stdflag_a->setChecked(false);
        ui->has_stdflag_p->setChecked(false);
        ui->has_stdflag_r->setChecked(false);
        ui->has_stdflag_n->setChecked(false);
        ui->has_stdflag_s->setChecked(false);
        ui->has_stdflag_f->setChecked(false);
    }
}

void SolverDialog::on_updateButton_clicked()
{
    if (ui->name->text().trimmed().isEmpty()) {
        QMessageBox::warning(this,"MiniZinc IDE","You need to specify a name for the solver.",QMessageBox::Ok);
        return;
    }
    if (ui->solverId->text().trimmed().isEmpty()) {
        QMessageBox::warning(this,"MiniZinc IDE","You need to specify a solver ID for the solver.",QMessageBox::Ok);
        return;
    }
    if (ui->executable->text().trimmed().isEmpty()) {
        QMessageBox::warning(this,"MiniZinc IDE","You need to specify an executable for the solver.",QMessageBox::Ok);
        return;
    }
    if (ui->version->text().trimmed().isEmpty()) {
        QMessageBox::warning(this,"MiniZinc IDE","You need to specify a version for the solver.",QMessageBox::Ok);
        return;
    }
    int index = ui->solvers_combo->currentIndex();
    for (int i=0; i<solvers.size(); i++) {
        if (i != index && ui->solverId->text().trimmed()==solvers[i].id) {
            QMessageBox::warning(this,"MiniZinc IDE","A solver with that solver ID already exists.",QMessageBox::Ok);
            return;
        }
    }
    if (index==solvers.size()) {
        Solver s;
        s.configFile = userSolverConfigDir+"/"+ui->solverId->text().trimmed()+".msc";
        solvers.append(s);
    }

    solvers[index].executable = ui->executable->text().trimmed();
    solvers[index].mznlib = ui->mznpath->text();
    solvers[index].name = ui->name->text().trimmed();
    solvers[index].id = ui->solverId->text().trimmed();
    solvers[index].version = ui->version->text().trimmed();
    solvers[index].isGUIApplication= ui->detach->isChecked();
    solvers[index].supportsMzn = !ui->needs_mzn2fzn->isChecked();

    solvers[index].stdFlags.removeAll("-a");
    if (ui->has_stdflag_a->isChecked()) {
        solvers[index].stdFlags.push_back("-a");
    }
    solvers[index].stdFlags.removeAll("-n");
    if (ui->has_stdflag_n->isChecked()) {
        solvers[index].stdFlags.push_back("-n");
    }
    solvers[index].stdFlags.removeAll("-p");
    if (ui->has_stdflag_p->isChecked()) {
        solvers[index].stdFlags.push_back("-p");
    }
    solvers[index].stdFlags.removeAll("-s");
    if (ui->has_stdflag_s->isChecked()) {
        solvers[index].stdFlags.push_back("-s");
    }
    solvers[index].stdFlags.removeAll("-r");
    if (ui->has_stdflag_r->isChecked()) {
        solvers[index].stdFlags.push_back("-r");
    }
    solvers[index].stdFlags.removeAll("-f");
    if (ui->has_stdflag_f->isChecked()) {
        solvers[index].stdFlags.push_back("-f");
    }

    QJsonObject json = solvers[index].json;
    json.remove("configFile");
    json["executable"] = ui->executable->text().trimmed();
    json["mznlib"] = ui->mznpath->text();
    json["name"] = ui->name->text().trimmed();
    json["id"] = ui->solverId->text().trimmed();
    json["version"] = ui->version->text().trimmed();
    json["isGUIApplication"] = ui->detach->isChecked();
    json["supportsMzn"] = !ui->needs_mzn2fzn->isChecked();
    json["stdFlags"] = QJsonArray::fromStringList(solvers[index].stdFlags);
    QJsonDocument jdoc(json);
    QFile jdocFile(solvers[index].configFile);
    if (!jdocFile.open(QIODevice::ReadWrite)) {
        QMessageBox::warning(this,"MiniZinc IDE","Cannot save configuration file "+solvers[index].configFile,QMessageBox::Ok);
        return;
    }
    if (jdocFile.write(jdoc.toJson())==-1) {
        QMessageBox::warning(this,"MiniZinc IDE","Cannot save configuration file "+solvers[index].configFile,QMessageBox::Ok);
        return;
    }

    if (index==solvers.size()-1) {
        ui->solvers_combo->insertItem(index,ui->name->text()+" "+ui->version->text(),index);
    }
    ui->solvers_combo->setCurrentIndex(index);
}

void SolverDialog::on_deleteButton_clicked()
{
    int index = ui->solvers_combo->currentIndex();
    if (QMessageBox::warning(this,"MiniZinc IDE","Delete solver "+solvers[index].name+"?",QMessageBox::Ok | QMessageBox::Cancel)
            == QMessageBox::Ok) {
        QFile sf(solvers[index].configFile);
        if (!sf.remove()) {
            QMessageBox::warning(this,"MiniZinc IDE","Cannot remove configuration file "+solvers[index].configFile,QMessageBox::Ok);
            return;
        }
        solvers.remove(index);
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
        on_mznDistribPath_returnPressed();
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

void SolverDialog::on_check_updates_stateChanged(int checkstate)
{
    QSettings settings;
    settings.beginGroup("ide");
    settings.setValue("checkforupdates21", checkstate==Qt::Checked);
    settings.endGroup();
}

void SolverDialog::checkMznExecutable(const QString& mznDistribPath,
                                      QString& mzn2fzn_executable,
                                      QString& mzn2fzn_version_string,                                      
                                      QVector<Solver>& solvers,
                                      QString& userSolverConfigDir,
                                      QString& mznStdlibDir)
{
    MznProcess p;
    QStringList args;
    args << "--version";
    mzn2fzn_executable = "";
    mzn2fzn_version_string = "";
    p.start("minizinc", args, mznDistribPath);
    if (!p.waitForStarted() || !p.waitForFinished()) {
        p.start("mzn2fzn.bat", args, mznDistribPath);
        if (!p.waitForStarted() || !p.waitForFinished()) {
            return;
        } else {
            mzn2fzn_executable = "mzn2fzn.bat";
        }
    } else {
        mzn2fzn_executable = "minizinc";
    }
    mzn2fzn_version_string = p.readAllStandardOutput()+p.readAllStandardError();
    if (!mzn2fzn_executable.isEmpty()) {
        args.clear();
        args << "--config-dirs";
        p.start(mzn2fzn_executable, args, mznDistribPath);
        if (p.waitForStarted() && p.waitForFinished()) {
            QString allOutput = p.readAllStandardOutput();
            QJsonDocument jd = QJsonDocument::fromJson(allOutput.toUtf8());
            if (!jd.isNull()) {
                QJsonObject sj = jd.object();
                userSolverConfigDir = sj["userSolverConfigDir"].toString();
                mznStdlibDir = sj["mznStdlibDir"].toString();
            }
        }
        args.clear();
        args << "--solvers-json";
        p.start(mzn2fzn_executable, args, mznDistribPath);
        if (p.waitForStarted() && p.waitForFinished()) {
            QString allOutput = p.readAllStandardOutput();
            QJsonDocument jd = QJsonDocument::fromJson(allOutput.toUtf8());
            if (!jd.isNull()) {
                solvers.clear();
                QJsonArray allSolvers = jd.array();
                for (int i=0; i<allSolvers.size(); i++) {
                    QJsonObject sj = allSolvers[i].toObject();
                    Solver s;
                    s.json = sj;
                    s.name = sj["name"].toString();
                    s.configFile = sj["configFile"].toString("");
                    s.contact = sj["contact"].toString("");
                    s.website = sj["website"].toString("");
                    s.supportsFzn = sj["supportsFzn"].toBool(true);
                    s.supportsMzn = sj["supportsMzn"].toBool(false);
                    if (sj["requiredFlags"].isArray()) {
                        QJsonArray rfs = sj["requiredFlags"].toArray();
                        for (auto rf : rfs) {
                            s.requiredFlags.push_back(rf.toString());
                        }
                    }
                    if (sj["stdFlags"].isArray()) {
                        QJsonArray sfs = sj["stdFlags"].toArray();
                        for (auto sf : sfs) {
                            s.stdFlags.push_back(sf.toString());
                        }
                    }
                    s.isGUIApplication = sj["isGUIApplication"].toBool(false);
                    s.needsMznExecutable = sj["needsMznExecutable"].toBool(false);
                    s.needsStdlibDir = sj["needsStdlibDir"].toBool(false);
                    s.executable = sj["executable"].toString("");
                    s.id = sj["id"].toString();
                    s.version = sj["version"].toString();
                    s.mznlib = sj["mznlib"].toString();
                    s.mznLibVersion = sj["mznlibVersion"].toInt();
                    solvers.append(s);
                }
            }
        }
    }
}

void SolverDialog::editingFinished(bool showError)
{
    QString mzn2fzn_executable;
    QString mzn2fzn_version;
    checkMznExecutable(ui->mznDistribPath->text(),mzn2fzn_executable,mzn2fzn_version,solvers,userSolverConfigDir,mznStdlibDir);
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

void SolverDialog::on_mznDistribPath_returnPressed()
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
    QProcess::start(program,arguments, QIODevice::Unbuffered | QIODevice::ReadWrite);
#ifdef Q_OS_WIN
    _putenv_s("PATH", curPath.toStdString().c_str());
#else
    setenv("PATH", curPath.toStdString().c_str(), 1);
#endif
}

void MznProcess::terminate()
{
#ifdef Q_OS_WIN
        AttachConsole(pid()->dwProcessId);
        SetConsoleCtrlHandler(NULL, TRUE);
        GenerateConsoleCtrlEvent(CTRL_C_EVENT, 0);
#else
        ::kill(processId(), SIGINT);
#endif
        if (!waitForFinished(500)) {
            if (state() != QProcess::NotRunning) {
#ifdef Q_OS_WIN
#else
                ::killpg(processId(), SIGKILL);
#endif
                if (!waitForFinished(500)) {
                    kill();
                    waitForFinished();
                }
            }
        }
}

void MznProcess::setupChildProcess()
{
#ifndef Q_OS_WIN
    if (::setpgid(0,0)) {
        std::cerr << "Error: Failed to create sub-process\n";
    }
#endif
}

void SolverDialog::on_check_solver_clicked()
{
    editingFinished(true);
}
