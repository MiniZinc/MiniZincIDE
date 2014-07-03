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

class Project : public QStandardItemModel
{
    Q_OBJECT
public:
    Project();
    void setRoot(QTreeView* treeView, const QString& fileName);
    void addFile(QTreeView* treeView, const QString& fileName);
    void removeFile(const QString& fileName);
    QList<QString> files(void) const { return _files.keys(); }
    QString fileAtIndex(const QModelIndex& index);
    virtual Qt::ItemFlags flags(const QModelIndex& index) const;
    QStringList dataFiles(void) const;
    void setRunningModel(const QModelIndex& index);
    void setEditable(const QModelIndex& index);
    virtual bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole );
    bool isProjectFile(const QModelIndex& index) { return projectFile->index()==index; }
    bool isModified() const { return _isModified; }
    void setModified(bool flag);
signals:
    void fileRenamed(const QString& oldName, const QString& newName);
    void modificationChanged(bool);
protected:
    bool _isModified;
    QString projectRoot;
    QMap<QString, QModelIndex> _files;
    QStandardItem* projectFile;
    QStandardItem* mzn;
    QStandardItem* dzn;
    QStandardItem* other;
    QModelIndex runningModel;
    QModelIndex editable;

};

#endif // PROJECT_H
