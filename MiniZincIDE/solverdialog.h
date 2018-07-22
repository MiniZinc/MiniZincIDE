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

#ifndef SOLVERDIALOG_H
#define SOLVERDIALOG_H

#include <QDialog>
#include <QProcess>
#include <QJsonObject>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace Ui {
class SolverDialog;
}

struct Solver {
    QString configFile;
    QString id;
    QString name;
    QString executable;
    QString executable_resolved;
    QString mznlib;
    QString mznlib_resolved;
    QString version;
    int mznLibVersion;
    QString description;
    QString contact;
    QString website;
    bool supportsMzn;
    bool supportsFzn;
    bool needsSolns2Out;
    bool isGUIApplication;
    bool needsMznExecutable;
    bool needsStdlibDir;
    bool needsPathsFile;
    QStringList stdFlags;
    QStringList extraFlags;
    QStringList requiredFlags;
    QStringList defaultFlags;
    QStringList tags;
    QJsonObject json;
    Solver(void) {}
};

class MznProcess : public QProcess {
#ifdef Q_OS_WIN
    Q_OBJECT
#endif
public:
    MznProcess(QObject* parent=NULL) : QProcess(parent) {}
    void start(const QString& program, const QStringList& arguments, const QString& path);
    void terminate(void);
protected:
    virtual void setupChildProcess();
#ifdef Q_OS_WIN
private:
    HANDLE jobObject;
private slots:
    void attachJob();
#endif
};

class SolverDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SolverDialog(QVector<Solver>& solvers,
                          QString& userSolverConfigDir,
                          QString& userConfigFile,
                          QString& mznStdlibDir,
                          bool openAsAddNew,
                          const QString& mznPath,
                          QWidget *parent = 0);
    ~SolverDialog();
    QString mznPath();
    static void checkMznExecutable(const QString& mznDistribPath,
                                   QString& mzn_executable,
                                   QString& mzn_version_string,
                                   QVector<Solver>& solvers,
                                   QString& userSolverConfigDir,
                                   QString& userConfigFile,
                                   QString& mznStdlibDir);
private slots:
    void on_solvers_combo_currentIndexChanged(int index);

    void on_updateButton_clicked();

    void on_deleteButton_clicked();

    void on_mznpath_select_clicked();

    void on_exec_select_clicked();

    void on_check_updates_stateChanged(int arg1);

    void on_mznDistribPath_returnPressed();

    void on_check_solver_clicked();

    void on_mznlib_select_clicked();

private:
    Ui::SolverDialog *ui;
    QVector<Solver>& solvers;
    QString& userSolverConfigDir;
    QString& userConfigFile;
    QString& mznStdlibDir;
    void editingFinished(bool showError);
};

#endif // SOLVERDIALOG_H
