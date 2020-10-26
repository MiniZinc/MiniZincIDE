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
#include "process.h"

#include <QFileInfo>
#include <QDir>
#include <QDebug>
#include <QMessageBox>
#include <QSortFilterProxyModel>
#include <QJsonDocument>
#include <QJsonArray>

Project::Project(const QList<SolverConfiguration*>& configs, QObject* parent) : QObject(parent), solverConfigs(configs)
{
    itemModel = new QStandardItemModel(this);
    itemModel->setColumnCount(1);

    connect(itemModel, &QStandardItemModel::itemChanged, this, &Project::on_itemChanged);

    rootItem = new QStandardItem(QIcon(":/images/mznicon.png"), "Untitled Project");
    rootItem->setData(NodeType::ProjectFile, Role::Type);
    rootItem->setData("Untitled Project", Role::OriginalLabel);
    rootItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable);

    auto font = rootItem->font();
    font.setBold(true);

    modelsItem = new QStandardItem("Models");
    modelsItem->setData(NodeType::Group, Role::Type);
    modelsItem->setFlags(Qt::NoItemFlags);
    modelsItem->setFont(font);

    dataItem = new QStandardItem("Data (right click to run)");
    dataItem->setData(NodeType::Group, Role::Type);
    dataItem->setFlags(Qt::NoItemFlags);
    dataItem->setFont(font);

    checkersItem = new QStandardItem("Checkers (right click to run)");
    checkersItem->setData(NodeType::Group, Role::Type);
    checkersItem->setFlags(Qt::NoItemFlags);
    checkersItem->setFont(font);

    configsItem = new QStandardItem("Solver configurations");
    configsItem->setData(NodeType::Group, Role::Type);
    configsItem->setFlags(Qt::NoItemFlags);
    configsItem->setFont(font);

    otherItem = new QStandardItem("Other files");
    otherItem->setData(NodeType::Group, Role::Type);
    otherItem->setFlags(Qt::NoItemFlags);
    otherItem->setFont(font);

    rootItem->appendRow(modelsItem);
    rootItem->appendRow(dataItem);
    rootItem->appendRow(checkersItem);
    rootItem->appendRow(configsItem);
    rootItem->appendRow(otherItem);
    itemModel->appendRow(rootItem);
}

QStringList Project::loadProject(const QString& file, ConfigWindow* configWindow)
{
    clear();

    projectFile(file);

    QStringList warnings;

    QFile f(file);
    QFileInfo fi(f);
    if (!f.open(QFile::ReadOnly)) {
        throw FileError("Failed to open project file");
    }

    auto doc = QJsonDocument::fromJson(f.readAll());

    if (doc.isObject()) {
        loadJSON(doc.object(), fi, configWindow, warnings);
    } else {
        f.reset();
        QDataStream in(&f);
        loadLegacy(in, fi, configWindow, warnings);
    }

    f.close();

    // Save these because adding the configs can change them
    auto projectBuiltinConfigId = selectedBuiltinConfigId;
    auto projectBuiltinConfigVersion = selectedBuiltinConfigVersion;
    auto projectselectedSolverConfigFile = selectedSolverConfigFile;

    for (auto& sc : solverConfigurationFiles()) {
        configWindow->addConfig(sc);
    }

    if (!projectBuiltinConfigId.isEmpty()) {
        int index = configWindow->findBuiltinConfig(projectBuiltinConfigId, projectBuiltinConfigVersion);
        if (index == -1) {
            warnings << "Could not find solver " + projectBuiltinConfigId + "@" + projectBuiltinConfigVersion;
        } else {
            configWindow->setCurrentIndex(index);
        }
    } else if (!projectselectedSolverConfigFile.isEmpty()) {
        int index = configWindow->findConfigFile(rootDir().absolutePath() + "/" + projectselectedSolverConfigFile);
        configWindow->setCurrentIndex(index);
    }

    setModified(false);

    return warnings;
}

void Project::loadJSON(const QJsonObject& obj, const QFileInfo& fi, ConfigWindow* configWindow, QStringList& warnings)
{
    int version = obj["version"].toInt();

    QString basePath = fi.absolutePath() + "/";

    auto of = obj["openFiles"].toArray();
    for (auto file : of) {
        auto path = basePath + file.toString();
        if (QFileInfo(path).exists()) {
            openTabs << path;
        } else {
            warnings << "The file " + file.toString() + " could not be found";
        }
    }

    openTabIndex = obj["openTab"].toInt();

    QList<SolverConfiguration*> configs;
    if (obj["builtinSolverConfigs"].isArray()) {
        for (auto config : obj["builtinSolverConfigs"].toArray()) {
            if (!config.isObject()) {
                warnings << "Failed to read solver builtin solver config";
                continue;
            }
            SolverConfiguration* loaded;
            if (version >= 106) {
                loaded = new (SolverConfiguration) (SolverConfiguration::loadJSON(QJsonDocument(config.toObject())));
            } else {
                loaded = new (SolverConfiguration) (SolverConfiguration::loadLegacy(QJsonDocument(config.toObject())));
            }
            loaded->isBuiltin = true;
            configs << loaded;
        }
    }

    if (obj["projectSolverConfigs"].isArray()) {
        for (auto config : obj["projectSolverConfigs"].toArray()) {
            if (!config.isObject()) {
                warnings << "Failed to read solver project solver config";
                continue;
            }
            auto loaded = new (SolverConfiguration) (SolverConfiguration::loadLegacy(QJsonDocument(config.toObject())));
            loaded->modified = true;
            configs << loaded;
        }
    }

    configWindow->mergeConfigs(configs);

    if (obj["selectedBuiltinConfigId"].isString()) {
        selectedBuiltinConfigId = obj["selectedBuiltinConfigId"].toString();
        selectedBuiltinConfigVersion = obj["selectedBuiltinConfigVersion"].toString();
        selectedSolverConfigFile = "";

    } else if (obj["selectedSolverConfigFile"].isString()) {
        selectedSolverConfigFile = obj["selectedSolverConfigFile"].toString();
        selectedBuiltinConfigId = "";
        selectedBuiltinConfigVersion = "";
    } /*else {
        warnings << "No selected solver config in project";
    }*/

    for (auto file : obj["projectFiles"].toArray()) {
        auto path = basePath + file.toString();
        if (QFileInfo(path).exists()) {
            add(path);
        } else {
            warnings << "The file " + file.toString() + " could not be found";
        }
    }
}

void Project::loadLegacy(QDataStream& in, const QFileInfo& fi, ConfigWindow* configWindow, QStringList& warnings)
{
    // Load old binary format of project
    throw InternalError("This project format is no longer supported. Please use MiniZinc IDE version 2.4 to upgrade it.");
}

void Project::saveProject()
{
    QJsonObject confObject;
    confObject["version"] = 106;

    // Save the currently open tabs
    QStringList of;
    for (auto& f: openTabs) {
         of << relativeToProject(f);
    }
    confObject["openFiles"] = QJsonArray::fromStringList(of);
    confObject["openTab"] = openTabIndex;

    // Save paths of all project files
    QStringList relativeFilePaths;
    for (auto& file : files()) {
        relativeFilePaths << relativeToProject(file);
    }
    confObject["projectFiles"] = QJsonArray::fromStringList(relativeFilePaths);

    // Save which config is currently selected
    if (!selectedBuiltinConfigId.isEmpty() && !selectedBuiltinConfigVersion.isEmpty()) {
        confObject["selectedBuiltinConfigId"] = selectedBuiltinConfigId;
        confObject["selectedBuiltinConfigVersion"] = selectedBuiltinConfigVersion;
    } else if (!selectedSolverConfigFile.isEmpty()){
        confObject["selectedSolverConfigFile"] = selectedSolverConfigFile;
    }

    // Write project file
    QJsonDocument doc(confObject);
    QFile file(projectFile());
    if (!file.open(QFile::WriteOnly)) {
        throw FileError("Failed to write file");
    }
    file.write(doc.toJson());
    file.close();

    setModified(false);
}

void Project::add(const QString& fileName)
{
    auto abs = QFileInfo(fileName).absoluteFilePath();
    auto path = relativeToProject(fileName);
    if (contains(abs)) {
        return;
    }

    auto parts = path.split("/", QString::SkipEmptyParts); // Qt always uses / as the path separator
    auto file = parts.takeLast();

    QStandardItem* node = otherItem;
    QString icon;
    NodeType type = NodeType::Other;
    if (file.endsWith(".mzc.mzn") || file.endsWith(".mzc")) {
        node = checkersItem;
        icon = ":/images/mznicon.png";
        type = NodeType::Checker;
    } else if (file.endsWith(".mzn")) {
        node = modelsItem;
        icon = ":/images/mznicon.png";
        type = NodeType::Model;
    } else if (file.endsWith(".dzn") || file.endsWith(".json")) {
        node = dataItem;
        icon = ":/images/mznicon.png";
        type = NodeType::Data;
    } else if (file.endsWith(".mpc")) {
        node = configsItem;
        icon = ":/images/mznicon.png";
        type = NodeType::SolverConfig;
    } else if (file == "_mooc" || file == "_coursera") {
        if (mooc) {
            delete mooc;
        }
        mooc = new MOOCAssignment(fileName);
        emit moocChanged(mooc);
    }

    node->setFlags(Qt::ItemIsEnabled);

    // Traverse existing path items
    int i = 0;
    while (!parts.empty() && i < node->rowCount()) {
        if (getType(node->child(i)->index()) == NodeType::Dir &&
                node->child(i)->text() == parts.first()) {
            parts.pop_front();
            node = node->child(i);
            i = 0;
        } else {
            i++;
        }
    }
    // Create new path items
    for (auto& part : parts) {
        auto dir = new QStandardItem(QIcon(":/icons/images/folder.png"), part);
        dir->setData(NodeType::Dir, Role::Type);
        dir->setFlags(Qt::ItemIsEnabled);
        node->appendRow(dir);
        node->sortChildren(0);
        node = dir;
    }
    // Add file item
    auto item = new QStandardItem(QIcon(icon), file);
    item->setData(type, Role::Type);
    item->setData(abs, Role::Path);
    item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable);
    node->appendRow(item);
    node->sortChildren(0);

    entries.insert(abs, item);

    if (!projectFile().isEmpty()) {
        setModified(true);
    }
}

void Project::add(const QStringList& fileNames)
{
    for (auto& fileName : fileNames) {
        add(fileName);
    }
}

void Project::remove(const QString &fileName)
{
    auto path = QFileInfo(fileName).absoluteFilePath();
    if (!contains(path)) {
        return;
    }

    // Make sure we also remove any unused dirs
    auto node = entries[path];
    if (node->text() == "_mooc" || node->text() == "_coursera") {
        delete mooc;
        mooc = nullptr;
        emit moocChanged(mooc);
    }
    while (getType(node->parent()->index()) == NodeType::Dir && node->parent()->rowCount() <= 1) {
        node = node->parent();
    }
    auto parent = node->parent();
    parent->removeRow(node->row());
    if (parent->data(Role::Type) == "group" && !parent->hasChildren()) {
        parent->setFlags(Qt::NoItemFlags);
    }
    entries.remove(path);

    if (!projectFile().isEmpty()) {
        setModified(true);
    }
}

void Project::remove(const QStringList &fileNames)
{
    for (auto& fileName : fileNames) {
        remove(fileName);
    }
}

void Project::remove(const QModelIndex& index)
{
    remove(getFileName(index));
}

void Project::remove(const QModelIndexList& indexes)
{
    for (auto& index : indexes) {
        remove(index);
    }
}

void Project::clear()
{
    modelsItem->removeRows(0, modelsItem->rowCount());
    dataItem->removeRows(0, dataItem->rowCount());
    checkersItem->removeRows(0, checkersItem->rowCount());
    otherItem->removeRows(0, otherItem->rowCount());
    entries.clear();
    setModified(true);
}

QStringList Project::files(void) const {
    return entries.keys();
}

QStringList Project::modelFiles(void) const {
    return getFiles(NodeType::Model);
}

QStringList Project::solverConfigurationFiles(void) const {
    return getFiles(NodeType::SolverConfig);
}

QStringList Project::dataFiles(void) const {
    return getFiles(NodeType::Data);
}

bool Project::contains(const QString &fileName)
{
    QFileInfo fi(fileName);
    return entries.contains(fi.absoluteFilePath());
}

void Project::setModified(bool m)
{
    modified = m;
    auto label = rootItem->data(Role::OriginalLabel).toString();
    if (modified) {
        rootItem->setText(label + " *");
    } else {
        rootItem->setText(label);
    }
}

void Project::projectFile(const QString& fileName)
{
    QStringList files = entries.keys();
    clear();
    if (fileName.isEmpty()) {
        rootItem->setText("Untitled Project");
        rootItem->setData("Untitled Project", Role::OriginalLabel);
        projFile = "";
    } else {
        QFileInfo fi(fileName);
        projFile = fi.absoluteFilePath();
        rootItem->setText(fi.fileName());
        rootItem->setData(fi.fileName(), Role::OriginalLabel);
    }
    add(files);
}

QDir Project::rootDir()
{
    QFileInfo fi(projectFile());
    return QDir(fi.absolutePath());
}

bool Project::hasProjectFile()
{
    return !projectFile().isEmpty();
}

Project::NodeType Project::getType(const QModelIndex& index)
{
    return static_cast<Project::NodeType>(model()->data(index, Role::Type).toInt());
}

QString Project::getFileName(const QModelIndex& index)
{
    return model()->data(index, Role::Path).toString();
}

QStringList Project::getFileNames(const QModelIndexList& indices)
{
    QStringList result;
    for (auto& index : indices) {
        result << getFileName(index);
    }
    return result;
}

QStringList Project::getFiles(NodeType type) const
{
    QStringList ret;
    for (auto it = entries.begin(); it != entries.end(); it++) {
        auto t = static_cast<NodeType>(it.value()->data(Role::Type).toInt());
        if (t == type) {
            ret << it.key();
        }
    }
    return ret;
}

QString Project::relativeToProject(const QString& fileName)
{
    QFileInfo fi(fileName);
    auto abs = fi.absoluteFilePath();
    return hasProjectFile() ? rootDir().relativeFilePath(abs) : abs;
}

void Project::openTabsChanged(const QStringList& files, int currentTab)
{
    openTabs.clear();
    for (auto& file : files) {
        auto abs = QFileInfo(file).absoluteFilePath();
        if (contains(abs)) {
            openTabs << abs;
        }
    }

    if (currentTab >= 0) {
        openTabIndex = openTabs.indexOf(QFileInfo(files[currentTab]).absoluteFilePath());
    } else {
        openTabIndex = -1;
    }

//    setModified(true);
}

void Project::activeSolverConfigChanged(const SolverConfiguration* sc)
{
    if (!sc) {
        selectedSolverConfigFile = "";
        selectedBuiltinConfigId = "";
        selectedBuiltinConfigVersion = "";
    }
    if (sc->isBuiltin) {
        selectedSolverConfigFile = "";
        selectedBuiltinConfigId = sc->solverDefinition.id;
        selectedBuiltinConfigVersion = sc->solverDefinition.version;
    } else {
        selectedBuiltinConfigId = "";
        selectedBuiltinConfigVersion = "";
        selectedSolverConfigFile = relativeToProject(sc->paramFile);
    }
//    setModified(true);
}

void Project::on_itemChanged(QStandardItem* item)
{
    auto oldPath = item->data(Qt::UserRole + 1).toString();
    auto newName = item->text();
    if (oldPath.isEmpty() || oldPath.endsWith(newName)) {
        return;
    }
    QFileInfo fi(oldPath);
    auto target = fi.path() + "/" + item->text();
    if (QFile::rename(oldPath, target)) {
        remove(oldPath);
        add(target);
        emit fileRenamed(oldPath, target);
    } else {
        item->setText(fi.fileName());
    }
}
