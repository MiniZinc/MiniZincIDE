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
#include "solverconfiguration.h"

class QSortFilterProxyModel;

class MOOCAssignmentItem {
public:
    QString id;
    QString model;
    QString data;
    int timeout;
    QString name;
    MOOCAssignmentItem(QString id0, QString model0, QString data0, QString timeout0, QString name0)
        : id(id0), model(model0), data(data0), timeout(timeout0.toInt()), name(name0) {}
    MOOCAssignmentItem(QString id0, QString model0, QString name0)
        : id(id0), model(model0), timeout(-1), name(name0) {}
};

class MOOCAssignment {
public:
    QString name;
    QString assignmentKey;
    QString moocName;
    QString moocPasswordString;
    QString submissionURL;
    QList<MOOCAssignmentItem> problems;
    QList<MOOCAssignmentItem> models;
};

class Project : public QStandardItemModel
{
    Q_OBJECT
public:
    Project(void);
    ~Project(void);
    void setRoot(QTreeView* treeView, QSortFilterProxyModel* sort, const QString& fileName);
    QVariant data(const QModelIndex &index, int role) const;
    void addFile(QTreeView* treeView, QSortFilterProxyModel* sort, const QString& fileName);
    void removeFile(const QString& fileName);
    bool containsFile(const QString& fileName);
    QList<QString> files(void) const { return _files.keys(); }
    QString fileAtIndex(const QModelIndex& index);
    virtual Qt::ItemFlags flags(const QModelIndex& index) const;
    QStringList dataFiles(void) const;
    virtual bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole );
    bool isProjectFile(const QModelIndex& index) { return projectFile->index()==index; }
    bool isModified() const { return _isModified; }
    void setModified(bool flag, bool files=false);

    MOOCAssignment& moocAssignment(void) { return _moocAssignment!=nullptr ? *_moocAssignment : *_courseraAssignment; }
    bool isUndefined(void) const;
    const QVector<SolverConfiguration>& solverConfigs(void) const;
    void solverConfigs(const QVector<SolverConfiguration>& sc,bool init);

    void save(const QString& filepath, const QStringList& openFiles, int openTab);
    void load(const QString& filepath, QStringList& openFiles, int& openTab, QStringList& missingFiles);
signals:
    void fileRenamed(const QString& oldName, const QString& newName);
    void modificationChanged(bool);
    void moocButtonChanged(bool visible, const QString& label, const QIcon& icon);
protected:
    bool _isModified;
    bool _filesModified;
    QString projectRoot;
    QMap<QString, QPersistentModelIndex> _files;
    QStandardItem* projectFile;
    QStandardItem* mzn;
    QStandardItem* dzn;
    QStandardItem* mpc;
    QStandardItem* other;
    QVector<SolverConfiguration> _solverConfigs;
    MOOCAssignment* _moocAssignment;
    MOOCAssignment* _courseraAssignment;
private:
    void loadLegacy(const QString& basePath, QDataStream& in, QStringList& openFiles, int& openTab, QStringList& missingFiles);
};

#endif // PROJECT_H
