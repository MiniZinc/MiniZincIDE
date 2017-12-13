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

#include "project.h"
#include "ui_mainwindow.h"
#include "moocsubmission.h"

#include <QFileInfo>
#include <QDir>
#include <QDebug>
#include <QMessageBox>
#include <QSortFilterProxyModel>
#include <QJsonDocument>
#include <QJsonArray>

Project::Project(Ui::MainWindow *ui0) : ui(ui0), _moocAssignment(NULL), _courseraAssignment(NULL)
{
    projectFile = new QStandardItem("Untitled Project");
    invisibleRootItem()->appendRow(projectFile);
    mzn = new QStandardItem("Models");
    QFont font = mzn->font();
    font.setBold(true);
    mzn->setFont(font);
    invisibleRootItem()->appendRow(mzn);
    dzn = new QStandardItem("Data (right-click to run)");
    dzn->setFont(font);
    invisibleRootItem()->appendRow(dzn);
    other = new QStandardItem("Other");
    other->setFont(font);
    invisibleRootItem()->appendRow(other);
    _isModified = false;
}

Project::~Project() {
    delete _moocAssignment;
    delete _courseraAssignment;
}

void Project::setRoot(QTreeView* treeView, QSortFilterProxyModel* sort, const QString &fileName)
{
    if (fileName == projectRoot)
        return;
    _isModified = true;
    projectFile->setText(QFileInfo(fileName).fileName());
    projectFile->setIcon(QIcon(":/images/mznicon.png"));
    QStringList allFiles = files();
    if (mzn->rowCount() > 0)
        mzn->removeRows(0,mzn->rowCount());
    if (dzn->rowCount() > 0)
        dzn->removeRows(0,dzn->rowCount());
    if (other->rowCount() > 0)
        other->removeRows(0,other->rowCount());
    projectRoot = fileName;
    _files.clear();
    for (QStringList::iterator it = allFiles.begin(); it != allFiles.end(); ++it) {
        addFile(treeView, sort, *it);
    }
}

QVariant Project::data(const QModelIndex &index, int role) const
{
    if (role==Qt::UserRole) {
        QStandardItem* item = itemFromIndex(index);
        if (item->parent()==NULL || item->parent()==invisibleRootItem()) {
            if (item==projectFile) {
                return "00 - project";
            }
            if (item==mzn) {
                return "01 - mzn";
            }
            if (item==dzn) {
                return "02 - dzn";
            }
            if (item==other) {
                return "03 - other";
            }
        }
        return QStandardItemModel::data(index,Qt::DisplayRole);
    } else {
        return QStandardItemModel::data(index,role);
    }
}

void Project::addFile(QTreeView* treeView, QSortFilterProxyModel* sort, const QString &fileName)
{
    if (_files.contains(fileName))
        return;
    QFileInfo fi(fileName);
    QString absFileName = fi.absoluteFilePath();
    QString relFileName;
    if (!projectRoot.isEmpty()) {
        QDir projectDir(QFileInfo(projectRoot).absoluteDir());
        relFileName = projectDir.relativeFilePath(absFileName);
    } else {
        relFileName = absFileName;
    }

    QStringList path = relFileName.split(QDir::separator());
    while (path.first().isEmpty()) {
        path.pop_front();
    }
    QStandardItem* curItem;
    bool isMiniZinc = true;
    bool isMOOC = false;
    if (fi.suffix()=="mzn") {
        curItem = mzn;
    } else if (fi.suffix()=="dzn") {
        curItem = dzn;
    } else if (fi.suffix()=="fzn") {
        return;
    } else {
        curItem = other;
        isMiniZinc = false;
        isMOOC = fi.baseName()=="_coursera" || fi.baseName()=="_mooc";
    }

    if (isMOOC) {

        if (fi.baseName()=="_coursera" && _courseraAssignment != NULL) {
            QMessageBox::warning(treeView,"MiniZinc IDE",
                                "Cannot add second Coursera options file",
                                QMessageBox::Ok);
            return;
        }
        if (fi.baseName()=="_mooc" && _moocAssignment != NULL) {
            QMessageBox::warning(treeView,"MiniZinc IDE",
                                "Cannot add second MOOC options file",
                                QMessageBox::Ok);
            return;
        }

        QFile metadata(absFileName);
        if (!metadata.open(QIODevice::ReadOnly)) {
            QMessageBox::warning(treeView,"MiniZinc IDE",
                                 "Cannot open MOOC options file",
                                 QMessageBox::Ok);
            return;
        }

        MOOCAssignment* moocA = new MOOCAssignment;

        // Try to read as JSON file (new format)

        QTextStream jsonIn(&metadata);
        QString jsonString = jsonIn.readAll();
        QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonString.toUtf8());
        if (jsonDoc.isNull()) {
            moocA->submissionURL = "https://www.coursera.org/api/onDemandProgrammingScriptSubmissions.v1";
            moocA->moocName = "Coursera";
            moocA->moocPasswordString = "Assignment token";
            // try old format
            QTextStream in(&jsonString);
            if (in.status() != QTextStream::Ok) {
                delete moocA;
                moocA = NULL;
                goto coursera_done;
            }
            moocA->assignmentKey = in.readLine();
            if (in.status() != QTextStream::Ok) {
                delete moocA;
                moocA = NULL;
                goto coursera_done;
            }
            moocA->name = in.readLine();
            QString nSolutions_s = in.readLine();
            int nSolutions = nSolutions_s.toInt();
            for (int i=0; i<nSolutions; i++) {
                if (in.status() != QTextStream::Ok) {
                    delete moocA;
                    moocA = NULL;
                    goto coursera_done;
                }
                QString line = in.readLine();
                QStringList tokens = line.split(", ");
                if (tokens.size() < 5) {
                    delete moocA;
                    moocA = NULL;
                    goto coursera_done;
                }
                MOOCAssignmentItem item(tokens[0].trimmed(),tokens[1].trimmed(),tokens[2].trimmed(),
                                  tokens[3].trimmed(),tokens[4].trimmed());
                moocA->problems.append(item);
            }
            if (in.status() != QTextStream::Ok) {
                delete moocA;
                moocA = NULL;
                goto coursera_done;
            }
            nSolutions_s = in.readLine();
            nSolutions = nSolutions_s.toInt();
            for (int i=0; i<nSolutions; i++) {
                if (in.status() != QTextStream::Ok) {
                    delete moocA;
                    moocA = NULL;
                    goto coursera_done;
                }
                QString line = in.readLine();
                QStringList tokens = line.split(", ");
                if (tokens.size() < 3) {
                    delete moocA;
                    moocA = NULL;
                    goto coursera_done;
                }
                MOOCAssignmentItem item(tokens[0].trimmed(),tokens[1].trimmed(),tokens[2].trimmed());
                moocA->models.append(item);
            }
        } else {
            bool hadError = false;
            if (jsonDoc.isObject()) {
                QJsonObject moocO = jsonDoc.object();
                if (moocO.isEmpty()) {
                    hadError = true;
                } else {
                    if (!moocO["assignmentKey"].isString() || !moocO["name"].isString() || !moocO["moocName"].isString() ||
                            !moocO["moocPasswordString"].isString() || !moocO["submissionURL"].isString() ||
                            !moocO["solutionAssignments"].isArray() || !moocO["modelAssignments"].isArray()) {
                        hadError = true;
                    } else {
                        moocA->assignmentKey = moocO["assignmentKey"].toString();
                        moocA->name = moocO["name"].toString();
                        moocA->moocName = moocO["moocName"].toString();
                        moocA->moocPasswordString = moocO["moocPasswordString"].toString();
                        moocA->submissionURL = moocO["submissionURL"].toString();
                        QJsonArray sols = moocO["solutionAssignments"].toArray();
                        for (int i=0; i<sols.size(); i++) {
                            QJsonObject solO = sols[i].toObject();
                            if (!sols[i].isObject() || !solO["id"].isString() || !solO["model"].isString() ||
                                    !solO["data"].isString() || (!solO["timeout"].isDouble() && !solO["timeout"].isString() ) || !solO["name"].isString()) {
                                hadError = true;
                            } else {
                                QString timeout = solO["timeout"].isDouble() ? QString::number(solO["timeout"].toInt()) : solO["timeout"].toString();
                                MOOCAssignmentItem item(solO["id"].toString(), solO["model"].toString(), solO["data"].toString(),
                                                        timeout, solO["name"].toString());
                                moocA->problems.append(item);
                            }
                        }
                        QJsonArray models = moocO["modelAssignments"].toArray();
                        for (int i=0; i<models.size(); i++) {
                            QJsonObject modelO = models[i].toObject();
                            MOOCAssignmentItem item(modelO["id"].toString(), modelO["model"].toString(), modelO["name"].toString());
                            moocA->models.append(item);
                        }
                    }
                }
            } else {
                hadError = true;
            }
            if (hadError) {
                QMessageBox::warning(treeView,"MiniZinc IDE",
                                     "MOOC options file contains errors",
                                     QMessageBox::Ok);
                delete moocA;
                moocA = NULL;
            }
        }

        if (fi.baseName()=="_coursera") {
            _courseraAssignment = moocA;
        } else {
            _moocAssignment = moocA;
        }
        if (moocA) {
            ui->actionSubmit_to_MOOC->setVisible(true);
            QString moocName = _moocAssignment ? _moocAssignment->moocName : _courseraAssignment->moocName;
            ui->actionSubmit_to_MOOC->setText("Submit to "+moocName);
            if (moocName=="Coursera") {
                ui->actionSubmit_to_MOOC->setIcon(QIcon(":/icons/images/coursera.png"));
            } else {
                ui->actionSubmit_to_MOOC->setIcon(QIcon(":/icons/images/application-certificate.png"));
            }

        }
    }
coursera_done:

    setModified(true, true);
    QStandardItem* prevItem = curItem;
    treeView->expand(sort->mapFromSource(curItem->index()));
    curItem = curItem->child(0);
    int i=0;
    while (curItem != NULL) {
        if (curItem->text() == path.first()) {
            path.pop_front();
            treeView->expand(sort->mapFromSource(curItem->index()));
            prevItem = curItem;
            curItem = curItem->child(0);
            i = 0;
        } else {
            i += 1;
            curItem = curItem->parent()->child(i);
        }
    }
    for (int i=0; i<path.size(); i++) {
        QStandardItem* newItem = new QStandardItem(path[i]);
        prevItem->appendRow(newItem);
        if (i<path.size()-1) {
            newItem->setIcon(QIcon(":/icons/images/folder.png"));
        } else {
            _files.insert(absFileName,newItem->index());
            if (isMiniZinc) {
                newItem->setIcon(QIcon(":/images/mznicon.png"));
            }
        }
        treeView->expand(sort->mapFromSource(newItem->index()));
        prevItem = newItem;
    }
}

QString Project::fileAtIndex(const QModelIndex &index)
{
    QStandardItem* item = itemFromIndex(index);
    if (item==NULL || item->hasChildren())
        return "";
    QString fileName;
    while (item != NULL && item->parent() != NULL && item->parent() != invisibleRootItem() ) {
        if (fileName.isEmpty())
            fileName = item->text();
        else
            fileName = item->text()+"/"+fileName;
        item = item->parent();
    }
    if (fileName.isEmpty())
        return "";
    if (!projectRoot.isEmpty()) {
        fileName = QFileInfo(projectRoot).absolutePath()+"/"+fileName;
    }
    QFileInfo fi(fileName);
    if (fi.canonicalFilePath().isEmpty())
        fileName = "/"+fileName;
    fileName = QFileInfo(fileName).canonicalFilePath();
    return fileName;
}

Qt::ItemFlags Project::flags(const QModelIndex& index) const
{
    QStandardItem* item = itemFromIndex(index);
    if (!item->hasChildren() && (item==mzn || item==dzn || item==other) )
        return Qt::ItemIsSelectable;
    return Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable;
}

QStringList Project::dataFiles(void) const
{
    QStringList ret;
    for (QMap<QString,QPersistentModelIndex>::const_iterator it = _files.begin(); it != _files.end(); ++it) {
        if (it.key().endsWith(".dzn"))
            ret << it.key();
    }
    return ret;
}

void Project::removeFile(const QString &fileName)
{
    if (fileName.isEmpty())
        return;
    if (!_files.contains(fileName)) {
        qDebug() << "Internal error: file " << fileName << " not in project";
        return;
    }
    setModified(true, true);
    QModelIndex index = _files[fileName];
    _files.remove(fileName);
    QStandardItem* cur = itemFromIndex(index);
    while (cur->parent() != NULL && cur->parent() != invisibleRootItem() && !cur->hasChildren()) {
        int row = cur->row();
        cur = cur->parent();
        cur->removeRow(row);
    }
    QFileInfo fi(fileName);
    if (fi.fileName()=="_coursera") {
        delete _courseraAssignment;
        _courseraAssignment = NULL;
        if (_moocAssignment==NULL) {
            ui->actionSubmit_to_MOOC->setVisible(false);
        }
    } else if (fi.fileName()=="_mooc") {
        delete _moocAssignment;
        _moocAssignment = NULL;
        if (_courseraAssignment!=NULL) {
            ui->actionSubmit_to_MOOC->setText("Submit to "+_courseraAssignment->moocName);
            ui->actionSubmit_to_MOOC->setIcon(QIcon(":/icons/images/coursera.png"));
        } else {
            ui->actionSubmit_to_MOOC->setVisible(false);
        }
    }
}

void Project::setModified(bool flag, bool files)
{
    if (!projectRoot.isEmpty()) {
        if (_isModified != flag) {
            emit modificationChanged(flag);
            _isModified = flag;
            if (files) {
                _filesModified = _isModified;
            }
            if (!_isModified) {
                currentDataFileIndex(currentDataFileIndex(),true);
                haveExtraArgs(haveExtraArgs(),true);
                extraArgs(extraArgs(),true);
                mzn2fznVerbose(mzn2fznVerbose(),true);
                mzn2fznPrintStats(mzn2fznPrintStats(),true);
                mzn2fznOptimize(mzn2fznOptimize(),true);
                currentSolver(currentSolver(),true);
                n_solutions(n_solutions(),true);
                n_compress_solutions(n_compress_solutions(),true);
                printAll(printAll(),true);
                defaultBehaviour(defaultBehaviour(),true);
                printStats(printStats(),true);
                haveSolverFlags(haveSolverFlags(),true);
                solverFlags(solverFlags(),true);
                n_threads(n_threads(),true);
                haveSeed(haveSeed(),true);
                seed(seed(),true);
                timeLimit(timeLimit(),true);
                solverVerbose(solverVerbose(),true);
            }
        }
    }
}

bool Project::setData(const QModelIndex& index, const QVariant& value, int role)
{
    QString oldName = itemFromIndex(index)->text();
    if (oldName==value.toString())
        return false;
    QString filePath = QFileInfo(fileAtIndex(index)).canonicalPath();
    bool success = QFile::rename(filePath+"/"+oldName,filePath+"/"+value.toString());
    if (success) {
        _files[filePath+"/"+value.toString()] = _files[filePath+"/"+oldName];
        _files.remove(filePath+"/"+oldName);
        setModified(true, true);
        emit fileRenamed(filePath+"/"+oldName,filePath+"/"+value.toString());
        return QStandardItemModel::setData(index,value,role);
    } else {
        return false;
    }
}

bool Project::haveExtraArgs(void) const
{
    return ui->conf_have_cmd_params->isChecked();
}
QString Project::extraArgs(void) const
{
    return ui->conf_cmd_params->text();
}
bool Project::haveExtraMzn2FznArgs(void) const
{
    return ui->conf_have_mzn2fzn_params->isChecked();
}
QString Project::extraMzn2FznArgs(void) const
{
    return ui->conf_mzn2fzn_params->text();
}
bool Project::autoClearOutput(void) const
{
    return ui->autoclear_output->isChecked();
}
bool Project::mzn2fznVerbose(void) const
{
    return ui->conf_verbose->isChecked();
}

bool Project::mzn2fznPrintStats() const
{
    return ui->conf_flatten_stats->isChecked();
}
bool Project::mzn2fznOptimize(void) const
{
    return ui->conf_optimize->isChecked();
}
QString Project::currentSolver(void) const
{
    return ui->conf_solver->currentText();
}
int Project::n_solutions(void) const
{
    return ui->conf_nsol->value();
}
int Project::n_compress_solutions(void) const
{
    return ui->conf_compressSolutionLimit->value();
}
bool Project::printAll(void) const
{
    return ui->conf_printall->isChecked();
}
bool Project::defaultBehaviour(void) const
{
    return ui->defaultBehaviourButton->isChecked();
}
bool Project::printStats(void) const
{
    return ui->conf_stats->isChecked();
}
bool Project::haveSolverFlags(void) const
{
    return ui->conf_have_solverFlags->isChecked();
}
QString Project::solverFlags(void) const
{
    return ui->conf_solverFlags->text();
}
int Project::n_threads(void) const
{
    return ui->conf_nthreads->value();
}
bool Project::haveSeed(void) const
{
    return ui->conf_have_seed->isChecked();
}
QString Project::seed(void) const
{
    return ui->conf_seed->text();
}
int Project::timeLimit(void) const
{
    return ui->conf_timeLimit->value();
}
bool Project::solverVerbose(void) const
{
    return ui->conf_solver_verbose->isChecked();
}

bool Project::isUndefined() const
{
    return projectRoot.isEmpty();
}

void Project::currentDataFileIndex(int i, bool init)
{
    if (init) {
        _currentDatafileIndex = i;
        ui->conf_data_file->setCurrentIndex(i);
    } else {
        if (i < ui->conf_data_file->count()-1)
            checkModified();
    }
}

void Project::haveExtraArgs(bool b, bool init)
{
    if (init) {
        _haveExtraArgs = b;
        ui->conf_have_cmd_params->setChecked(b);
    } else {
        checkModified();
    }
}

void Project::extraArgs(const QString& a, bool init)
{
    if (init) {
        _extraArgs = a;
        ui->conf_cmd_params->setText(a);
    } else {
        checkModified();
    }
}

void Project::haveExtraMzn2FznArgs(bool b, bool init)
{
    if (init) {
        _haveExtraMzn2FznArgs = b;
        ui->conf_have_mzn2fzn_params->setChecked(b);
    } else {
        checkModified();
    }
}

void Project::extraMzn2FznArgs(const QString& a, bool init)
{
    if (init) {
        _extraMzn2FznArgs = a;
        ui->conf_mzn2fzn_params->setText(a);
    } else {
        checkModified();
    }
}

void Project::autoClearOutput(bool b, bool init)
{
    if (init) {
        _autoclear_output= b;
        ui->autoclear_output->setChecked(b);
    } else {
        checkModified();
    }
}

void Project::mzn2fznVerbose(bool b, bool init)
{
    if (init) {
        _mzn2fzn_verbose= b;
        ui->conf_verbose->setChecked(b);
    } else {
        checkModified();
    }
}

void Project::mzn2fznPrintStats(bool b, bool init)
{
    if (init) {
        _mzn2fzn_printStats = b;
        ui->conf_flatten_stats->setChecked(b);
    } else {
        checkModified();
    }
}

void Project::mzn2fznOptimize(bool b, bool init)
{
    if (init) {
        _mzn2fzn_optimize = b;
        ui->conf_optimize->setChecked(b);
    } else {
        checkModified();
    }
}

void Project::currentSolver(const QString& s, bool init)
{
    if (init) {
        _currentSolver = s;
        ui->conf_solver->setCurrentText(s);
    } else {
        checkModified();
    }
}

void Project::n_solutions(int n, bool init)
{
    if (init) {
        _n_solutions = n;
        ui->conf_nsol->setValue(n);
    } else {
        checkModified();
    }
}

void Project::n_compress_solutions(int n, bool init)
{
    if (init) {
        _compressSolutionLimit = n;
        ui->conf_compressSolutionLimit->setValue(n);
    } else {
        checkModified();
    }
}

void Project::printAll(bool b, bool init)
{
    if (init) {
        _printAll = b;
        ui->conf_printall->setChecked(b);
    } else {
        checkModified();
    }
}

void Project::defaultBehaviour(bool b, bool init)
{
    if (init) {
        _defaultBehaviour = b;
        ui->defaultBehaviourButton->setChecked(b);
    } else {
        checkModified();
    }
}

void Project::printStats(bool b, bool init)
{
    if (init) {
        _printStats = b;
        ui->conf_stats->setChecked(b);
    } else {
        checkModified();
    }
}

void Project::haveSolverFlags(bool b, bool init)
{
    if (init) {
        _haveSolverFlags = b;
        ui->conf_have_solverFlags->setChecked(b);
    } else {
        checkModified();
    }
}

void Project::solverFlags(const QString& s, bool init)
{
    if (init) {
        _solverFlags = s;
        ui->conf_solverFlags->setText(s);
    } else {
        checkModified();
    }
}

void Project::n_threads(int n, bool init)
{
    if (init) {
        _n_threads = n;
        ui->conf_nthreads->setValue(n);
    } else {
        checkModified();
    }
}

void Project::haveSeed(bool b, bool init)
{
    if (init) {
        _haveSeed = b;
        ui->conf_have_seed->setChecked(b);
    } else {
        checkModified();
    }
}

void Project::seed(const QString& s, bool init)
{
    if (init) {
        _seed = s;
        ui->conf_seed->setText(s);
    } else {
        checkModified();
    }
}

void Project::timeLimit(int n, bool init)
{
    if (init) {
        _timeLimit = n;
        ui->conf_timeLimit->setValue(n);
    } else {
        checkModified();
    }
}

void Project::solverVerbose(bool b, bool init)
{
    if (init) {
        _solverVerbose = b;
        ui->conf_solver_verbose->setChecked(b);
    } else {
        checkModified();
    }
}

int Project::currentDataFileIndex(void) const
{
    return ui->conf_data_file->currentIndex();
}

QString Project::currentDataFile(void) const
{
    return ui->conf_data_file->currentText();
}

void Project::checkModified()
{
    if (projectRoot.isEmpty() || _filesModified)
        return;
    if (currentDataFileIndex() != _currentDatafileIndex) {
        setModified(true);
        return;
    }
    if (haveExtraArgs() != _haveExtraArgs) {
        setModified(true);
        return;
    }
    if (extraArgs() != _extraArgs) {
        setModified(true);
        return;
    }
    if (mzn2fznVerbose() != _mzn2fzn_verbose) {
        setModified(true);
        return;
    }
    if (mzn2fznPrintStats() != _mzn2fzn_printStats) {
        setModified(true);
        return;
    }
    if (mzn2fznOptimize() != _mzn2fzn_optimize) {
        setModified(true);
        return;
    }
    if (currentSolver() != _currentSolver) {
        setModified(true);
        return;
    }
    if (n_solutions() != _n_solutions) {
        setModified(true);
        return;
    }
    if (n_compress_solutions() != _compressSolutionLimit) {
        setModified(true);
        return;
    }
    if (printAll() != _printAll) {
        setModified(true);
        return;
    }
    if (defaultBehaviour() != _defaultBehaviour) {
        setModified(true);
        return;
    }
    if (printStats() != _printStats) {
        setModified(true);
        return;
    }
    if (haveSolverFlags() != _haveSolverFlags) {
        setModified(true);
        return;
    }
    if (solverFlags() != _solverFlags) {
        setModified(true);
        return;
    }
    if (n_threads() != _n_threads) {
        setModified(true);
        return;
    }
    if (haveSeed() != _haveSeed) {
        setModified(true);
        return;
    }
    if (seed() != _seed) {
        setModified(true);
        return;
    }
    if (timeLimit() != _timeLimit) {
        setModified(true);
        return;
    }
    if (solverVerbose() != _solverVerbose) {
        setModified(true);
        return;
    }
    setModified(false);
}
