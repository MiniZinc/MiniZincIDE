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

namespace Ui {
    class MainWindow;
}

class CourseraItem {
public:
    QString id;
    QString model;
    QString data;
    int timeout;
    QString name;
    CourseraItem(QString id0, QString model0, QString data0, QString timeout0, QString name0)
        : id(id0), model(model0), data(data0), timeout(timeout0.toInt()), name(name0) {}
    CourseraItem(QString id0, QString model0, QString name0)
        : id(id0), model(model0), timeout(-1), name(name0) {}
};

class CourseraProject {
public:
    QString name;
    QString course;
    QList<CourseraItem> submissions;
};

class Project : public QStandardItemModel
{
    Q_OBJECT
public:
    Project(Ui::MainWindow *ui0);
    ~Project(void);
    void setRoot(QTreeView* treeView, const QString& fileName);
    void addFile(QTreeView* treeView, const QString& fileName);
    void removeFile(const QString& fileName);
    QList<QString> files(void) const { return _files.keys(); }
    QString fileAtIndex(const QModelIndex& index);
    virtual Qt::ItemFlags flags(const QModelIndex& index) const;
    QStringList dataFiles(void) const;
    void setEditable(const QModelIndex& index);
    virtual bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole );
    bool isProjectFile(const QModelIndex& index) { return projectFile->index()==index; }
    bool isModified() const { return _isModified; }
    void setModified(bool flag, bool files=false);

    int currentDataFileIndex(void) const;
    QString currentDataFile(void) const;
    bool haveExtraArgs(void) const;
    QString extraArgs(void) const;
    bool haveExtraMzn2FznArgs(void) const;
    QString extraMzn2FznArgs(void) const;
    bool mzn2fznVerbose(void) const;
    bool mzn2fznOptimize(void) const;
    QString currentSolver(void) const;
    int n_solutions(void) const;
    bool printAll(void) const;
    bool printStats(void) const;
    bool haveSolverFlags(void) const;
    QString solverFlags(void) const;
    int n_threads(void) const;
    bool haveSeed(void) const;
    QString seed(void) const;
    int timeLimit(void) const;
    bool solverVerbose(void) const;
    CourseraProject& coursera(void) { return *_courseraProject; }
public slots:
    void currentDataFileIndex(int i, bool init=false);
    void haveExtraArgs(bool b, bool init=false);
    void extraArgs(const QString& a, bool init=false);
    void haveExtraMzn2FznArgs(bool b, bool init=false);
    void extraMzn2FznArgs(const QString& a, bool init=false);
    void mzn2fznVerbose(bool b, bool init=false);
    void mzn2fznOptimize(bool b, bool init=false);
    void currentSolver(const QString& s, bool init=false);
    void n_solutions(int n, bool init=false);
    void printAll(bool b, bool init=false);
    void printStats(bool b, bool init=false);
    void haveSolverFlags(bool b, bool init=false);
    void solverFlags(const QString& s, bool init=false);
    void n_threads(int n, bool init=false);
    void haveSeed(bool b, bool init=false);
    void seed(const QString& s, bool init=false);
    void timeLimit(int n, bool init=false);
    void solverVerbose(bool b, bool init=false);
signals:
    void fileRenamed(const QString& oldName, const QString& newName);
    void modificationChanged(bool);
protected:
    Ui::MainWindow *ui;
    bool _isModified;
    bool _filesModified;
    QString projectRoot;
    QMap<QString, QModelIndex> _files;
    QStandardItem* projectFile;
    QStandardItem* mzn;
    QStandardItem* dzn;
    QStandardItem* other;
    QModelIndex editable;

    int _currentDatafileIndex;
    bool _haveExtraArgs;
    QString _extraArgs;
    bool _haveExtraMzn2FznArgs;
    QString _extraMzn2FznArgs;
    bool _mzn2fzn_verbose;
    bool _mzn2fzn_optimize;
    QString _currentSolver;
    int _n_solutions;
    bool _printAll;
    bool _printStats;
    bool _haveSolverFlags;
    QString _solverFlags;
    int _n_threads;
    bool _haveSeed;
    QString _seed;
    int _timeLimit;
    bool _solverVerbose;
    CourseraProject* _courseraProject;

    void checkModified(void);
};

#endif // PROJECT_H
