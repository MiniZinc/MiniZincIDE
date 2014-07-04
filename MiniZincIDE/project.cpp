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

#include <QFileInfo>
#include <QDir>
#include <QDebug>

Project::Project(Ui::MainWindow *ui0) : ui(ui0)
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

void Project::setRoot(QTreeView* treeView, const QString &fileName)
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
        addFile(treeView, *it);
    }
}

void Project::addFile(QTreeView* treeView, const QString &fileName)
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
    if (fi.suffix()=="mzn") {
        curItem = mzn;
    } else if (fi.suffix()=="dzn") {
        curItem = dzn;
    } else if (fi.suffix()=="fzn") {
        return;
    } else {
        curItem = other;
        isMiniZinc = false;
    }
    setModified(true, true);
    QStandardItem* prevItem = curItem;
    treeView->expand(curItem->index());
    curItem = curItem->child(0);
    int i=0;
    while (curItem != NULL) {
        if (curItem->text() == path.first()) {
            path.pop_front();
            treeView->expand(curItem->index());
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
        treeView->expand(newItem->index());
        prevItem = newItem;
    }
}

QString Project::fileAtIndex(const QModelIndex &index)
{
    QStandardItem* item = itemFromIndex(index);
    if (item->hasChildren())
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
    if (index==editable) {
        return Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable;
    } else {
        QStandardItem* item = itemFromIndex(index);
        if (!item->hasChildren() && (item==mzn || item==dzn || item==other) )
            return Qt::ItemIsSelectable;
        return Qt::ItemIsSelectable | Qt::ItemIsEnabled;
    }
}

QStringList Project::dataFiles(void) const
{
    QStringList ret;
    for (QMap<QString,QModelIndex>::const_iterator it = _files.begin(); it != _files.end(); ++it) {
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
}

void Project::setEditable(const QModelIndex &index)
{
    editable = index;
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
        }
    }
}

bool Project::setData(const QModelIndex& index, const QVariant& value, int role)
{
    editable = QModelIndex();
    QString oldName = itemFromIndex(index)->text();
    if (oldName==value.toString())
        return false;
    QString filePath = QFileInfo(fileAtIndex(index)).canonicalPath();
    bool success = QFile::rename(filePath+"/"+oldName,filePath+"/"+value.toString());
    if (success) {
        _files[value.toString()] = _files[oldName];
        _files.remove(oldName);
        emit fileRenamed(filePath+"/"+oldName,filePath+"/"+value.toString());
        return QStandardItemModel::setData(index,value,role);
    } else {
        return false;
    }
}

void Project::currentDataFileIndex(int i, bool init)
{
    if (init) {
        _currentDatafileIndex = i;
        ui->conf_data_file->setCurrentIndex(i);
    } else {
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

void Project::mzn2fznVerbose(bool b, bool init)
{
    if (init) {
        _mzn2fzn_verbose= b;
        ui->conf_verbose->setChecked(b);
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

void Project::printAll(bool b, bool init)
{
    if (init) {
        _printAll = b;
        ui->conf_printall->setChecked(b);
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
    if (currentDataFileIndex() != ui->conf_data_file->currentIndex()) {
        setModified(true);
        return;
    }
    if (haveExtraArgs() != ui->conf_have_cmd_params->isChecked()) {
        setModified(true);
        return;
    }
    if (extraArgs() != ui->conf_cmd_params->text()) {
        setModified(true);
        return;
    }
    if (mzn2fznVerbose() != ui->conf_verbose->isChecked()) {
        setModified(true);
        return;
    }
    if (mzn2fznOptimize() != ui->conf_optimize->isChecked()) {
        setModified(true);
        return;
    }
    if (currentSolver() != ui->conf_solver->currentText()) {
        setModified(true);
        return;
    }
    if (n_solutions() != ui->conf_nsol->value()) {
        setModified(true);
        return;
    }
    if (printAll() != ui->conf_printall->isChecked()) {
        setModified(true);
        return;
    }
    if (printStats() != ui->conf_stats->isChecked()) {
        setModified(true);
        return;
    }
    if (haveSolverFlags() != ui->conf_have_solverFlags->isChecked()) {
        setModified(true);
        return;
    }
    if (solverFlags() != ui->conf_solverFlags->text()) {
        setModified(true);
        return;
    }
    if (n_threads() != ui->conf_nthreads->value()) {
        setModified(true);
        return;
    }
    if (haveSeed() != ui->conf_have_seed->isChecked()) {
        setModified(true);
        return;
    }
    if (seed() != ui->conf_seed->text()) {
        setModified(true);
        return;
    }
    if (timeLimit() != ui->conf_timeLimit->value()) {
        setModified(true);
        return;
    }
    if (solverVerbose() != ui->conf_solver_verbose->isChecked()) {
        setModified(true);
        return;
    }
    setModified(false);
}
