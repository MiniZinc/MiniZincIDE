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
#include "moocsubmission.h"
#include "solverdialog.h"
#include "exception.h"
#include "process.h"

#include <QFileInfo>
#include <QDir>
#include <QDebug>
#include <QMessageBox>
#include <QSortFilterProxyModel>
#include <QJsonDocument>
#include <QJsonArray>

Project::Project() : _moocAssignment(nullptr), _courseraAssignment(nullptr)
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
    mpc = new QStandardItem("Solver Configurations");
    mpc->setFont(font);
    invisibleRootItem()->appendRow(mpc);
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
    if (mpc->rowCount() > 0)
        mpc->removeRows(0,mpc->rowCount());
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
        if (item->parent()==nullptr || item->parent()==invisibleRootItem()) {
            if (item==projectFile) {
                return "00 - project";
            }
            if (item==mzn) {
                return "01 - mzn";
            }
            if (item==dzn) {
                return "02 - dzn";
            }
            if (item==mpc) {
                return "04 - mpc";
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
    } else if (fi.suffix()=="dzn" || fi.suffix()=="json") {
        curItem = dzn;
    } else if (fi.suffix()=="fzn") {
        return;
    } else if (fi.suffix()=="mpc") {
        curItem = mpc;
    } else {
        curItem = other;
        isMiniZinc = false;
        isMOOC = fi.baseName()=="_coursera" || fi.baseName()=="_mooc";
    }

    if (isMOOC) {

        if (fi.baseName()=="_coursera" && _courseraAssignment != nullptr) {
            QMessageBox::warning(treeView,"MiniZinc IDE",
                                "Cannot add second Coursera options file",
                                QMessageBox::Ok);
            return;
        }
        if (fi.baseName()=="_mooc" && _moocAssignment != nullptr) {
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
                moocA = nullptr;
                goto coursera_done;
            }
            moocA->assignmentKey = in.readLine();
            if (in.status() != QTextStream::Ok) {
                delete moocA;
                moocA = nullptr;
                goto coursera_done;
            }
            moocA->name = in.readLine();
            QString nSolutions_s = in.readLine();
            int nSolutions = nSolutions_s.toInt();
            for (int i=0; i<nSolutions; i++) {
                if (in.status() != QTextStream::Ok) {
                    delete moocA;
                    moocA = nullptr;
                    goto coursera_done;
                }
                QString line = in.readLine();
                QStringList tokens = line.split(", ");
                if (tokens.size() < 5) {
                    delete moocA;
                    moocA = nullptr;
                    goto coursera_done;
                }
                QFileInfo model_fi(fi.dir(), tokens[1].trimmed());
                QFileInfo data_fi(fi.dir(), tokens[2].trimmed());
                MOOCAssignmentItem item(tokens[0].trimmed(),model_fi.absoluteFilePath(),data_fi.absoluteFilePath(),
                                  tokens[3].trimmed(),tokens[4].trimmed());
                moocA->problems.append(item);
            }
            if (in.status() != QTextStream::Ok) {
                delete moocA;
                moocA = nullptr;
                goto coursera_done;
            }
            nSolutions_s = in.readLine();
            nSolutions = nSolutions_s.toInt();
            for (int i=0; i<nSolutions; i++) {
                if (in.status() != QTextStream::Ok) {
                    delete moocA;
                    moocA = nullptr;
                    goto coursera_done;
                }
                QString line = in.readLine();
                QStringList tokens = line.split(", ");
                if (tokens.size() < 3) {
                    delete moocA;
                    moocA = nullptr;
                    goto coursera_done;
                }
                QFileInfo model_fi(fi.dir(), tokens[1].trimmed());
                MOOCAssignmentItem item(tokens[0].trimmed(),model_fi.absoluteFilePath(),tokens[2].trimmed());
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
                                QFileInfo model_fi(fi.dir(), solO["model"].toString());
                                QFileInfo data_fi(fi.dir(), solO["data"].toString());
                                MOOCAssignmentItem item(solO["id"].toString(), model_fi.absoluteFilePath(), data_fi.absoluteFilePath(),
                                                        timeout, solO["name"].toString());
                                moocA->problems.append(item);
                            }
                        }
                        QJsonArray models = moocO["modelAssignments"].toArray();
                        for (int i=0; i<models.size(); i++) {
                            QJsonObject modelO = models[i].toObject();
                            QFileInfo model_fi(fi.dir(), modelO["model"].toString());
                            MOOCAssignmentItem item(modelO["id"].toString(), model_fi.absoluteFilePath(), modelO["name"].toString());
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
                moocA = nullptr;
            }
        }

        if (fi.baseName()=="_coursera") {
            _courseraAssignment = moocA;
        } else {
            _moocAssignment = moocA;
        }
        if (moocA) {
            QString moocName = _moocAssignment ? _moocAssignment->moocName : _courseraAssignment->moocName;
            QString label = "Submit to " + moocName;
            QIcon icon = moocName == "Coursera" ? QIcon(":/icons/images/coursera.png") : QIcon(":/icons/images/application-certificate.png");
            emit moocButtonChanged(true, label, icon);
        }
    }
coursera_done:

    setModified(true, true);
    QStandardItem* prevItem = curItem;
    treeView->expand(sort->mapFromSource(curItem->index()));
    curItem = curItem->child(0);
    int i=0;
    while (curItem != nullptr) {
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
    if (item==nullptr || item->hasChildren())
        return "";
    QString fileName;
    while (item != nullptr && item->parent() != nullptr && item->parent() != invisibleRootItem() ) {
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
    if (!item->hasChildren() && (item==mzn || item==dzn || item==mpc || item==other) )
        return Qt::ItemIsSelectable;
    return Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable;
}

QStringList Project::dataFiles(void) const
{
    QStringList ret;
    for (QMap<QString,QPersistentModelIndex>::const_iterator it = _files.begin(); it != _files.end(); ++it) {
        if (it.key().endsWith(".dzn") || it.key().endsWith(".json"))
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
    while (cur->parent() != nullptr && cur->parent() != invisibleRootItem() && !cur->hasChildren()) {
        int row = cur->row();
        cur = cur->parent();
        cur->removeRow(row);
    }
    QFileInfo fi(fileName);
    if (fi.fileName()=="_coursera") {
        delete _courseraAssignment;
        _courseraAssignment = nullptr;
        if (_moocAssignment==nullptr) {
            moocButtonChanged(false, "", QIcon());
        }
    } else if (fi.fileName()=="_mooc") {
        delete _moocAssignment;
        _moocAssignment = nullptr;
        if (_courseraAssignment!=nullptr) {
            moocButtonChanged(true, "Submit to " + _courseraAssignment->moocName, QIcon(":/icons/images/coursera.png"));
        } else {
            moocButtonChanged(false, "", QIcon());
        }
    }
}

bool Project::containsFile(const QString &fileName)
{
    return _files.contains(fileName);
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
                solverConfigs(solverConfigs(),true);
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

const QVector<SolverConfiguration>& Project::solverConfigs(void) const {
    return _solverConfigs;
}

bool Project::isUndefined() const
{
    return projectRoot.isEmpty();
}

void Project::solverConfigs(const QVector<SolverConfiguration> &sc, bool init)
{
//    if (init) {
//        _solverConfigs = sc;
//    } else {
//        if (sc.size() != _solverConfigs.size()) {
//            setModified(true);
//        } else {
//            bool modified = false;
//            for (int i=0; i<sc.size(); i++) {
//                if (!(sc[i]==_solverConfigs[i])) {
//                    modified = true;
//                    _solverConfigs[i] = sc[i];
//                }
//            }
//            setModified(modified);
//        }
//    }
}


void Project::save(const QString& filepath, const QStringList& openFiles, int openTab)
{
    QFile file(filepath);
    if (!file.open(QFile::WriteOnly | QFile::Text)) {
        throw ProjectError("Failed to save project");
    }
    QJsonObject confObject;

    confObject["version"] = 106;
    confObject["openFiles"] = QJsonArray::fromStringList(openFiles);
    confObject["openTab"] = openTab;

    QDir projectDir = QFileInfo(filepath).absoluteDir();

    QStringList projectFilesRelPath;
    QStringList projectFiles = files();
    for (QList<QString>::const_iterator it = projectFiles.begin();
         it != projectFiles.end(); ++it) {
        projectFilesRelPath << projectDir.relativeFilePath(*it);
    }
    confObject["projectFiles"] = QJsonArray::fromStringList(projectFilesRelPath);

    QJsonDocument jdoc(confObject);
    file.write(jdoc.toJson());
    file.close();
}

namespace {
    // Upgrades a solver config from a project file into new format
    SolverConfiguration scFromJson(QJsonObject sco) {
        QString solver = sco["version"].isString() ? sco["id"].toString() + "@" + sco["version"].toString() : sco["id"].toString();
        SolverConfiguration newSc(Solver::lookup(solver));
        bool defaultBehaviour = false;
        if (sco["name"].isString()) {
//            newSc.name = sco["name"].toString();
        }
        if (sco["timeLimit"].isDouble()) {
            newSc.timeLimit = sco["timeLimit"].toInt();
        }
        if (sco["defaultBehavior"].isBool()) {
            defaultBehaviour = sco["defaultBehavior"].toBool();
        }
        if (sco["printIntermediate"].isBool()) {
            newSc.printIntermediate = sco["printIntermediate"].toBool();
        }
        if (sco["stopAfter"].isDouble()) {
            newSc.numSolutions = sco["stopAfter"].toInt();
        }
        if (sco["verboseFlattening"].isBool()) {
            newSc.verboseCompilation = sco["verboseFlattening"].toBool();
        }
        if (sco["flatteningStats"].isBool()) {
            newSc.compilationStats = sco["flatteningStats"].toBool();
        }
        if (sco["optimizationLevel"].isDouble()) {
            newSc.optimizationLevel = sco["optimizationLevel"].toInt();
        }
        if (sco["additionalData"].isString()) {
            newSc.additionalData.append(sco["additionalData"].toString());
        }
        if (sco["additionalCompilerCommandline"].isString()) {
            // TODO: Implement
        }
        if (sco["nThreads"].isDouble()) {
            newSc.numThreads = sco["nThreads"].toInt();
        }
        if (sco["randomSeed"].isDouble()) {
            newSc.randomSeed = sco["randomSeed"].toDouble();
        }
        if (sco["solverFlags"].isString()) {
            // TODO: Implement
        }
        if (sco["freeSearch"].isBool()) {
            newSc.freeSearch = sco["freeSearch"].toBool();
        }
        if (sco["verboseSolving"].isBool()) {
            newSc.verboseSolving = sco["verboseSolving"].toBool();
        }
        if (sco["outputTiming"].isBool()) {
            newSc.outputTiming = sco["outputTiming"].toBool();
        }
        if (sco["solvingStats"].isBool()) {
            newSc.solvingStats = sco["solvingStats"].toBool();
        }
        if (sco["useExtraOptions"].isBool()) {
            newSc.useExtraOptions = sco["useExtraOptions"].toBool();
        }
        if (sco["extraOptions"].isObject()) {
            QJsonObject extraOptions = sco["extraOptions"].toObject();
            for (auto& k : extraOptions.keys()) {
                newSc.extraOptions.insert(k, extraOptions[k].toString());
            }
        }
        return newSc;
    }
}

void Project::load(const QString& filepath, QStringList& openFiles, int& openTab, QStringList& missingFiles)
{
    QFile pfile(filepath);
    pfile.open(QIODevice::ReadOnly);
    if (!pfile.isOpen()) {
        throw FileError("Could not open project file");
    }
    QString basePath = QFileInfo(filepath).absolutePath() + "/";
    QByteArray jsonData = pfile.readAll();
    QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonData);
    if (!jsonDoc.isObject()) {
        pfile.reset();
        QDataStream in(&pfile);
        loadLegacy(basePath, in, openFiles, openTab, missingFiles);
        return;
    }



    QJsonObject obj = jsonDoc.object();
    if (obj["openFiles"].isArray()) {
        for (auto f : obj["openFiles"].toArray()) {
            if (!f.isString()) {
                throw ProjectError("Invalid project file");
            }
            openFiles << f.toString();
        }
    }
    openTab = obj["openTab"].toInt(-1);

    QStringList projectFilesRelPath;
    if (obj["projectFiles"].isArray()) {
        for (auto f : obj["projectFiles"].toArray()) {
            if (!f.isString()) {
                throw ProjectError("Invalid project file");
            }
            projectFilesRelPath << f.toString();
        }
    }



}

void Project::loadLegacy(const QString& basePath, QDataStream& in, QStringList &openFiles, int &openTab, QStringList& missingFiles)
{
    quint32 magic;
    in >> magic;
    if (magic != 0xD539EA12) {
        throw ProjectError("Invalid project file");
    }
    quint32 version;
    in >> version;
    if (version < 101 || version > 104) {
        throw ProjectError("Unsupported version");
    }
    in.setVersion(QDataStream::Qt_5_0);

    QString prefix = version >= 103 ? basePath : "";

    in >> openFiles;
    QString p_s;
    bool p_b;

    int dataFileIndex;

    Solver* s = MznDriver::get().defaultSolver();
    if (!s) {
        throw ConfigError("No default solver found");
    }
    SolverConfiguration newConf(*s);

    in >> p_s;
    in >> dataFileIndex;
    in >> p_b;
    in >> p_s;
    newConf.additionalData << p_s;
    in >> p_b;
    // Ignore, not used any longer
    in >> p_s;
    // TODO: Implement turning into unknownOptions
    if (version == 104) {
        in >> p_b; // Ignore clear output window option
    }
    in >> newConf.verboseCompilation;
    in >> p_b;
    newConf.optimizationLevel = p_b ? 1 : 0;
    in >> p_s; // Used to be solver name
    in >> newConf.numSolutions;
    in >> newConf.printIntermediate;
    in >> newConf.solvingStats;
    in >> p_b; // Ignore
    in >> p_s; // TODO: This is solver flags
    in >> newConf.numThreads;
    in >> p_b;
    in >> p_s;
    newConf.randomSeed = p_b ? QVariant::fromValue(p_s.toInt()) : QVariant();
    in >> p_b; // used to be whether time limit is checked
    in >> newConf.timeLimit;
    openTab = -1;
    if (version >= 102) {
        in >> newConf.verboseSolving;
        in >> openTab;
    }
    QStringList projectFilesRelPath;
    if (version==103 || version==104) {
        in >> projectFilesRelPath;
    } else {
        projectFilesRelPath = openFiles;
    }
    if (version >= 103 && !in.atEnd()) {
        in >> p_b; // Ignore default behaviour option
    }
    if (version == 104 && !in.atEnd()) {
        in >> newConf.compilationStats;
    }
    if (version == 104 && !in.atEnd()) {
        in >> p_b; // Ignore compress solution option
    }
    if (version==104 && !in.atEnd()) {
        in >> newConf.outputTiming;
    }


}
