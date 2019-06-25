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
#include <QGridLayout>
#include <QLineEdit>

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
                           QString& userConfigFile0,
                           QString& mznStdlibDir0,
                           bool openAsAddNew,
                           const QString& mznPath, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SolverDialog),
    solvers(solvers0), userSolverConfigDir(userSolverConfigDir0), userConfigFile(userConfigFile0), mznStdlibDir(mznStdlibDir0)
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
    on_solvers_combo_currentIndexChanged(0);
}

SolverDialog::~SolverDialog()
{
    delete ui;
}

QString SolverDialog::mznPath()
{
    return ui->mznDistribPath->text();
}

namespace {

    void clearRequiredFlagsLayout(QGridLayout* l) {
        QVector<QLayoutItem*> items;
        for (int i=0; i<l->rowCount(); i++) {
            for (int j=0; j<l->columnCount(); j++) {
                if (QLayoutItem* li = l->itemAtPosition(i,j)) {
                    if (QWidget* w = li->widget()) {
                        w->hide();
                        w->deleteLater();
                    }
                    items.push_back(li);
                }
            }
        }
        for (auto i : items) {
            l->removeItem(i);
            delete i;
        }
    }

}

void SolverDialog::on_solvers_combo_currentIndexChanged(int index)
{
    QGridLayout* rfLayout = static_cast<QGridLayout*>(ui->requiredFlags->layout());
    clearRequiredFlagsLayout(rfLayout);
    QString userSolverConfigCanonical = QFileInfo(userSolverConfigDir).canonicalPath();
    if (index<solvers.size()) {
        ui->name->setText(solvers[index].name);
        ui->solverId->setText(solvers[index].id);
        ui->version->setText(solvers[index].version);
        ui->executable->setText(solvers[index].executable);
        ui->exeNotFoundLabel->setVisible(!solvers[index].executable.isEmpty() && solvers[index].executable_resolved.isEmpty());
        ui->detach->setChecked(solvers[index].isGUIApplication);
        ui->needs_mzn2fzn->setChecked(!solvers[index].supportsMzn);
        ui->needs_solns2out->setChecked(solvers[index].needsSolns2Out);
        ui->mznpath->setText(solvers[index].mznlib);
        ui->updateButton->setText("Update");
        bool solverConfigIsUserEditable = false;
        if (!solvers[index].configFile.isEmpty()) {
            QFileInfo configFileInfo(solvers[index].configFile);
            if (configFileInfo.canonicalPath().startsWith(userSolverConfigCanonical)) {
                solverConfigIsUserEditable = true;
            }
        }

        ui->updateButton->setEnabled(solverConfigIsUserEditable || solvers[index].requiredFlags.size() != 0);
        ui->deleteButton->setEnabled(solverConfigIsUserEditable);
        ui->solverFrame->setEnabled(solverConfigIsUserEditable);

        ui->has_stdflag_a->setChecked(solvers[index].stdFlags.contains("-a"));
        ui->has_stdflag_p->setChecked(solvers[index].stdFlags.contains("-p"));
        ui->has_stdflag_r->setChecked(solvers[index].stdFlags.contains("-r"));
        ui->has_stdflag_n->setChecked(solvers[index].stdFlags.contains("-n"));
        ui->has_stdflag_s->setChecked(solvers[index].stdFlags.contains("-s"));
        ui->has_stdflag_f->setChecked(solvers[index].stdFlags.contains("-f"));
        ui->has_stdflag_v->setChecked(solvers[index].stdFlags.contains("-v"));
        ui->has_stdflag_t->setChecked(solvers[index].stdFlags.contains("-t"));

        if (solvers[index].requiredFlags.size()==0) {
            ui->requiredFlags->hide();
        } else {
            ui->requiredFlags->show();
            int row = 0;
            for (auto& rf : solvers[index].requiredFlags) {
                QString val;
                int foundFlag = solvers[index].defaultFlags.indexOf(rf);
                if (foundFlag != -1 && foundFlag < solvers[index].defaultFlags.size()-1) {
                    val = solvers[index].defaultFlags[foundFlag+1];
                }
                rfLayout->addWidget(new QLabel(rf), row, 0);
                rfLayout->addWidget(new QLineEdit(val), row, 1);
                row++;
            }
        }
    } else {
        ui->name->setText("");
        ui->solverId->setText("");
        ui->version->setText("");
        ui->executable->setText("");
        ui->detach->setChecked(false);
        ui->needs_mzn2fzn->setChecked(true);
        ui->needs_solns2out->setChecked(true);
        ui->mznpath->setText("");
        ui->solverFrame->setEnabled(true);
        ui->updateButton->setText("Add");
        ui->updateButton->setEnabled(true);
        ui->deleteButton->setEnabled(false);
        ui->has_stdflag_a->setChecked(false);
        ui->has_stdflag_p->setChecked(false);
        ui->has_stdflag_r->setChecked(false);
        ui->has_stdflag_n->setChecked(false);
        ui->has_stdflag_s->setChecked(false);
        ui->has_stdflag_v->setChecked(false);
        ui->has_stdflag_f->setChecked(false);
        ui->has_stdflag_t->setChecked(false);
        ui->requiredFlags->hide();
    }
}

void SolverDialog::on_updateButton_clicked()
{
    if (!ui->requiredFlags->isHidden()) {
        // This is an internal solver, we are only updating the required flags

        QGridLayout* rfLayout = static_cast<QGridLayout*>(ui->requiredFlags->layout());
        QVector<QStringList> defaultFlags;
        for (int row=0; row<rfLayout->rowCount(); row++) {
            if (QLayoutItem* li = rfLayout->itemAtPosition(row, 0)) {
                QString key = static_cast<QLabel*>(li->widget())->text();
                QLayoutItem* li2 = rfLayout->itemAtPosition(row, 1);
                QString val = static_cast<QLineEdit*>(li2->widget())->text();
                if (!val.isEmpty()) {
                    QStringList sl({ui->solverId->text(), key, val});
                    defaultFlags.push_back(sl);
                }
            }

        }

        QFile uc(userConfigFile);
        QJsonObject jo;
        if (uc.exists()) {
            if (uc.open(QFile::ReadOnly)) {
                QJsonDocument doc = QJsonDocument::fromJson(uc.readAll());
                jo = doc.object();
                uc.close();
            }
        }
        QJsonArray a0;
        if (!jo.contains("solverDefaults")) {
            // Fresh object, initialise
            for (auto& df : defaultFlags) {
                QJsonArray a1;
                for (auto& f : df) {
                    a1.append(f);
                }
                a0.append(a1);
            }
        } else {
            QJsonArray previous = jo["solverDefaults"].toArray();
            // First remove all entries for this solver
            for (auto triple : previous) {
                if (triple.isArray()) {
                    QJsonArray previousA1 = triple.toArray();
                    if (previousA1.size()==3) {
                        if (previousA1[0]!=ui->solverId->text()) {
                            a0.append(triple);
                        }
                    }
                }
            }
            // Now add the new values
            for (auto& df : defaultFlags) {
                QJsonArray a1;
                for (auto& f : df) {
                    a1.append(f);
                }
                a0.append(a1);
            }
        }
        jo["solverDefaults"] = a0;
        QJsonDocument doc;
        doc.setObject(jo);
        QFileInfo uc_info(userConfigFile);
        if (!QDir().mkpath(uc_info.absoluteDir().absolutePath())) {
            QMessageBox::warning(this,"MiniZinc IDE","Cannot create user configuration directory "+uc_info.absoluteDir().absolutePath(),QMessageBox::Ok);
            return;
        }
        if (uc.open(QFile::ReadWrite | QIODevice::Truncate)) {
            uc.write(doc.toJson());
            uc.close();
            editingFinished(false);
        } else {
            QMessageBox::warning(this,"MiniZinc IDE","Cannot write user configuration file "+userConfigFile,QMessageBox::Ok);
        }

    } else {

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
            if (!QDir().mkpath(userSolverConfigDir)) {
                QMessageBox::warning(this,"MiniZinc IDE","Cannot create user configuration directory "+userSolverConfigDir,QMessageBox::Ok);
                return;
            }
            solvers.append(s);
        }

        solvers[index].executable = ui->executable->text().trimmed();
        solvers[index].mznlib = ui->mznpath->text();
        solvers[index].name = ui->name->text().trimmed();
        solvers[index].id = ui->solverId->text().trimmed();
        solvers[index].version = ui->version->text().trimmed();
        solvers[index].isGUIApplication= ui->detach->isChecked();
        solvers[index].supportsMzn = !ui->needs_mzn2fzn->isChecked();
        solvers[index].needsSolns2Out = ui->needs_solns2out->isChecked();

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
        solvers[index].stdFlags.removeAll("-v");
        if (ui->has_stdflag_v->isChecked()) {
            solvers[index].stdFlags.push_back("-v");
        }
        solvers[index].stdFlags.removeAll("-r");
        if (ui->has_stdflag_r->isChecked()) {
            solvers[index].stdFlags.push_back("-r");
        }
        solvers[index].stdFlags.removeAll("-f");
        if (ui->has_stdflag_f->isChecked()) {
            solvers[index].stdFlags.push_back("-f");
        }
        solvers[index].stdFlags.removeAll("-t");
        if (ui->has_stdflag_t->isChecked()) {
            solvers[index].stdFlags.push_back("-t");
        }

        QJsonObject json = solvers[index].json;
        json.remove("extraInfo");
        json["executable"] = ui->executable->text().trimmed();
        json["mznlib"] = ui->mznpath->text();
        json["name"] = ui->name->text().trimmed();
        json["id"] = ui->solverId->text().trimmed();
        json["version"] = ui->version->text().trimmed();
        json["isGUIApplication"] = ui->detach->isChecked();
        json["supportsMzn"] = !ui->needs_mzn2fzn->isChecked();
        json["needsSolns2Out"] = ui->needs_solns2out->isChecked();
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
    editingFinished(false);
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
                                      QString& minizinc_executable,
                                      QString& minizinc_version_string,
                                      QVector<Solver>& solvers,
                                      QString& userSolverConfigDir,
                                      QString& userConfigFile,
                                      QString& mznStdlibDir)
{
    MznProcess p;
    QStringList args;
    args << "--version";
    minizinc_executable = "";
    minizinc_version_string = "";
    p.start("minizinc", args, mznDistribPath);
    if (!p.waitForStarted() || !p.waitForFinished()) {
        return;
    } else {
        minizinc_executable = "minizinc";
    }
    minizinc_version_string = p.readAllStandardOutput()+p.readAllStandardError();
    QRegExp reVersion("version ([0-9]+)\\.([0-9]+)\\.\\S+");
    if (reVersion.indexIn(minizinc_version_string)==-1) {
        minizinc_executable = "";
        return;
    } else {
        bool ok;
        int curVersionMajor = reVersion.cap(1).toInt(&ok);
        if (ok) {
            int curVersionMinor = reVersion.cap(2).toInt(&ok);
            if (curVersionMajor<2 || (curVersionMajor<3 && curVersionMinor<2)) {
                minizinc_executable = "";
                return;
            }
        }
    }
    if (!minizinc_executable.isEmpty()) {
        args.clear();
        args << "--config-dirs";
        p.start(minizinc_executable, args, mznDistribPath);
        if (p.waitForStarted() && p.waitForFinished()) {
            QString allOutput = p.readAllStandardOutput();
            QJsonDocument jd = QJsonDocument::fromJson(allOutput.toUtf8());
            if (!jd.isNull()) {
                QJsonObject sj = jd.object();
                userSolverConfigDir = sj["userSolverConfigDir"].toString();
                userConfigFile = sj["userConfigFile"].toString();
                mznStdlibDir = sj["mznStdlibDir"].toString();
            }
        }
        args.clear();
        args << "--solvers-json";
        p.start(minizinc_executable, args, mznDistribPath);
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
                    QJsonObject extraInfo = sj["extraInfo"].toObject();
                    s.executable_resolved = extraInfo["executable"].toString("");
                    s.mznlib_resolved = extraInfo["mznlib"].toString("");
                    s.configFile = extraInfo["configFile"].toString("");
                    if (extraInfo["defaultFlags"].isArray()) {
                        QJsonArray ei = extraInfo["defaultFlags"].toArray();
                        for (auto df : ei) {
                            s.defaultFlags.push_back(df.toString());
                        }
                    }
                    s.isDefaultSolver = (extraInfo["isDefault"].isBool() && extraInfo["isDefault"].toBool());
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
                    if (sj["extraFlags"].isArray()) {
                        QJsonArray sfs = sj["extraFlags"].toArray();
                        for (auto sf : sfs) {
                            if (sf.isArray()) {
                                QJsonArray extraFlagA = sf.toArray();
                                if (extraFlagA.size()==4) {
                                    SolverFlag extraFlag;
                                    extraFlag.min=1.0;
                                    extraFlag.max=0.0;
                                    extraFlag.name = extraFlagA[0].toString();
                                    QRegularExpression re_opt("^(int|float|bool)(:([0-9a-zA-Z.-]+):([0-9a-zA-Z.-]+))?");
                                    QRegularExpressionMatch re_opt_match = re_opt.match(extraFlagA[2].toString());
                                    if (re_opt_match.hasMatch()) {
                                        if (re_opt_match.captured(1)=="int") {
                                            if (re_opt_match.captured(3).isEmpty()) {
                                                extraFlag.t = SolverFlag::T_INT;
                                            } else {
                                                extraFlag.t = SolverFlag::T_INT_RANGE;
                                                extraFlag.min = re_opt_match.captured(3).toInt();
                                                extraFlag.max = re_opt_match.captured(4).toInt();
                                            }
                                        } else if (re_opt_match.captured(1)=="float") {
                                            if (re_opt_match.captured(3).isEmpty()) {
                                                extraFlag.t = SolverFlag::T_FLOAT;
                                            } else {
                                                extraFlag.t = SolverFlag::T_FLOAT_RANGE;
                                                extraFlag.min = re_opt_match.captured(3).toDouble();
                                                extraFlag.max = re_opt_match.captured(4).toDouble();
                                            }
                                        } else if (re_opt_match.captured(1)=="bool") {
                                            if (re_opt_match.captured(3).isEmpty()) {
                                                extraFlag.t = SolverFlag::T_BOOL;
                                            } else {
                                                extraFlag.t = SolverFlag::T_BOOL_ONOFF;
                                                extraFlag.options = QStringList({re_opt_match.captured(3),re_opt_match.captured(4)});
                                            }
                                        }
                                    } else {
                                        if (extraFlagA[2].toString()=="string") {
                                            extraFlag.t = SolverFlag::T_STRING;
                                        } else if (extraFlagA[2].toString().startsWith("opt:")) {
                                            extraFlag.t = SolverFlag::T_OPT;
                                            extraFlag.options = extraFlagA[2].toString().mid(4).split(":");
//                                        } else if (extraFlagA[2].toString()=="solver") {
//                                            extraFlag.t = SolverFlag::T_SOLVER;
                                        } else {
                                            continue;
                                        }
                                    }
                                    extraFlag.description = extraFlagA[1].toString();
                                    extraFlag.def = extraFlagA[3].toString();
                                    s.extraFlags.push_back(extraFlag);
                                }
                            }
                        }
                    }
                    s.isGUIApplication = sj["isGUIApplication"].toBool(false);
                    s.needsMznExecutable = sj["needsMznExecutable"].toBool(false);
                    s.needsStdlibDir = sj["needsStdlibDir"].toBool(false);
                    s.needsPathsFile = sj["needsPathsFile"].toBool(false);
                    s.needsSolns2Out = sj["needsSolns2Out"].toBool(true);
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
    QString minizinc_executable;
    QString minizinc_version;
    for (int i=0; i<solvers.size(); i++) {
        ui->solvers_combo->removeItem(0);
    }
    solvers.clear();
    checkMznExecutable(ui->mznDistribPath->text(),minizinc_executable,minizinc_version,solvers,userSolverConfigDir,userConfigFile,mznStdlibDir);
    if (minizinc_executable.isEmpty()) {
        if (showError) {
            QMessageBox::warning(this,"MiniZinc IDE","Could not find the minizinc executable.",
                                 QMessageBox::Ok);
        }
        if (minizinc_version.isEmpty()) {
            ui->mzn2fzn_version->setText("None");
        } else {
            ui->mzn2fzn_version->setText("None. Error message:\n"+minizinc_version);
        }
    } else {
        ui->mzn2fzn_version->setText(minizinc_version);
        for (int i=solvers.size(); i--;) {
            ui->solvers_combo->insertItem(0,solvers[i].name+" "+solvers[i].version,i);
        }
        ui->solvers_combo->setCurrentIndex(0);
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
    jobObject = CreateJobObject(NULL, NULL);
    connect(this, SIGNAL(started()), this, SLOT(attachJob()));
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
        ::killpg(processId(), SIGINT);
#endif
        if (!waitForFinished(500)) {
            if (state() != QProcess::NotRunning) {
#ifdef Q_OS_WIN
                TerminateJobObject(jobObject, EXIT_FAILURE);
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

#ifdef Q_OS_WIN
void MznProcess::attachJob()
{
    AssignProcessToJobObject(jobObject, pid()->hProcess);
}
#endif

void SolverDialog::on_check_solver_clicked()
{
    editingFinished(true);
}

void SolverDialog::on_mznlib_select_clicked()
{
    QFileDialog fd(this,"Select solver library path");
    QDir dir(ui->mznpath->text());
    fd.setDirectory(dir);
    fd.setFileMode(QFileDialog::Directory);
    fd.setOption(QFileDialog::ShowDirsOnly, true);
    if (fd.exec()) {
        ui->mznpath->setText(fd.selectedFiles().first());
    }

}
