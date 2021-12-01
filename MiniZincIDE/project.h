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

#ifndef PROJECT_H
#define PROJECT_H

#include <QSet>
#include <QStandardItemModel>
#include <QTreeView>
#include "solver.h"
#include "moocsubmission.h"
#include "exception.h"
#include "configwindow.h"

class Project : public QObject {
    Q_OBJECT

public:
    enum NodeType {
        Model = 1,
        Data = 2,
        Checker = 4,
        SolverConfig = 8,
        Other = 16,
        Dir = 32,
        Group = 64,
        ProjectFile = 128
    };
    Q_DECLARE_FLAGS(NodeTypes, NodeType)

    explicit Project(const QList<SolverConfiguration*>& configs, QObject* parent = nullptr);

    ~Project()
    {
        delete mooc;
        emit moocChanged(nullptr);
    }

    ///
    /// \brief Loads a project from an mzp file.
    /// \param file The mzp file to be loaded
    /// \param configWindow The solver configuration window
    /// \return A list of warnings from loading the project
    ///
    QStringList loadProject(const QString& file, ConfigWindow* configWindow);

    ///
    /// \brief Save this project
    ///
    void saveProject(void);

    ///
    /// \brief Add a file to the project
    /// \param fileName The file to be added
    ///
    void add(const QString& fileName);
    ///
    /// \brief Adds multiple files to the project
    /// \param fileNames The files to be added
    ///
    void add(const QStringList& fileNames);

    ///
    /// \brief Removes a files from the project
    /// \param fileName The file to be removed
    ///
    void remove(const QString& fileName);
    ///
    /// \brief Removes multiple files from the project
    /// \param fileNames The files to be removed
    ///
    void remove(const QStringList& fileNames);
    ///
    /// \brief Removes a file from the project
    /// \param index The index of the file to be removed
    ///
    void remove(const QModelIndex& index);
    ///
    /// \brief Removes multiple files from the project
    /// \param indexes The indices of the files to be removed
    ///
    void remove(const QModelIndexList& indexes);

    ///
    /// \brief Removes all files from the project
    ///
    void clear(void);

    ///
    /// \brief Gets the files in this project
    /// \return The files in this project
    ///
    QStringList files(void) const;

    ///
    /// \brief Gets the model files in this project (excluding checkers)
    /// \return The file paths of the model files
    ///
    QStringList modelFiles(void) const;

    ///
    /// \brief Gets the solver configuration files in this project
    /// \return The file paths of the solver configurations
    ///
    QStringList solverConfigurationFiles(void) const;

    ///
    /// \brief Gets the data files in this project
    /// \return The file paths of the data files
    ///
    QStringList dataFiles(void) const;

    const QStringList& openFiles(void) { return openTabs; }

    int openTab(void) { return openTabIndex; }

    ///
    /// \brief Return whether this project contains the given file
    /// \param fileName The file to check
    /// \return True if the given file is in the project, and false otherwise
    ///
    bool contains(const QString& fileName);

    ///
    /// \brief Return whether this project is modified
    /// \return True if this project was modified
    ///
    bool isModified(void)
    {
        return modified;
    }
    ///
    /// \brief Marks this project as modified or unmodified
    /// \param m True for modified, false for unmodified
    ///
    void setModified(bool m);

    ///
    /// \brief The path to the project .mzp file
    /// \return The path to the project file, or empty if there is none
    ///
    const QString& projectFile(void) const
    {
        return projFile;
    }
    ///
    /// \brief Set the project .mzp file
    /// \param fileName The new project file path
    ///
    void projectFile(const QString& fileName);

    ///
    /// \brief The root directory off of which project file paths are based
    /// \return The root directory
    ///
    QDir rootDir(void);
    ///
    /// \brief Return whether this project has a .mzp file
    /// \return True if there is one, and false otherwise
    ///
    bool hasProjectFile(void);

    ///
    /// \brief Gets the mooc assignment associated with this project
    /// \return The mooc assignment (throws exception if there is none)
    ///
    MOOCAssignment& moocAssignment(void)
    {
        if (!mooc) {
            throw MoocError("No mooc assignment in project");
        }
        return *mooc;
    }

    ///
    /// \brief Get the type of the node at the given index
    /// \param index The model index
    /// \return The type of the node
    ///
    NodeType getType(const QModelIndex& index);

    ///
    /// \brief Gets a file path from a model index
    /// \param index The model index
    /// \return The file path as a string
    ///
    QString getFileName(const QModelIndex& index);

    ///
    /// \brief Gets the file paths of the given model indexes
    /// \param indices The model indexes
    /// \return The full file paths
    ///
    QStringList getFileNames(const QModelIndexList& indices);

    ///
    /// \brief Get the underlying item model associated with this project
    /// \return The project files as a QAbstractItemModel
    ///
    QAbstractItemModel* model(void)
    {
        return itemModel;
    }

signals:
    ///
    /// \brief Emitted when a file gets renamed
    /// \param oldName The previous name of the file
    /// \param newName The new name of the file
    ///
    void fileRenamed(const QString& oldName, const QString& newName);
    void moocChanged(const MOOCAssignment* mooc);
public slots:
    void openTabsChanged(const QStringList& files, int currentTab);
    void activeSolverConfigChanged(const SolverConfiguration* sc);
private slots:
    void on_itemChanged(QStandardItem* item);
private:
    enum Role {
        Type = Qt::UserRole,
        Path = Qt::UserRole + 1,
        OriginalLabel = Qt::UserRole + 2
    };

    void loadJSON(const QJsonObject& obj, const QFileInfo& fi, ConfigWindow* configWindow, QStringList& warnings);
    void loadLegacy(QDataStream& in, const QFileInfo& fi, ConfigWindow* configWindow, QStringList& warnings);
    QStringList getFiles(NodeType type) const;

    QString relativeToProject(const QString& fileName);

    bool modified = false;

    QStandardItemModel* itemModel;

    QString projFile;

    QStandardItem* rootItem;
    QStandardItem* modelsItem;
    QStandardItem* checkersItem;
    QStandardItem* dataItem;
    QStandardItem* configsItem;
    QStandardItem* otherItem;

    MOOCAssignment* mooc = nullptr;

    QStringList openTabs;
    int openTabIndex;

    QString selectedBuiltinConfigId;
    QString selectedBuiltinConfigVersion;
    QString selectedSolverConfigFile;

    const QList<SolverConfiguration*>& solverConfigs;

    QMap<QString, QStandardItem*> entries;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(Project::NodeTypes)

#endif // PROJECT_H
