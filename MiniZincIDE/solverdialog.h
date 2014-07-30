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

namespace Ui {
class SolverDialog;
}

struct Solver {
    QString name;
    QString executable;
    QString mznlib;
    QString backend;
    bool builtin;
    bool detach;
    Solver(const QString& n, const QString& e, const QString& m, const QString& b, bool bi, bool dt) :
        name(n), executable(e), mznlib(m), backend(b), builtin(bi), detach(dt) {}
    Solver(void) {}
};

class MznProcess : public QProcess {
public:
    MznProcess(QObject* parent=NULL) : QProcess(parent) {}
    void start(const QString& program, const QStringList& arguments, const QString& path);
};

class SolverDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SolverDialog(QVector<Solver>& solvers, const QString& def,
                          bool openAsAddNew,
                          const QString& mznPath,
                          QWidget *parent = 0);
    ~SolverDialog();
    QString mznPath();
    QString def();
    static void checkMzn2fznExecutable(const QString& mznDistribPath,
                                       QString& mzn2fzn_executable,
                                       QString& mzn2fzn_version_string);
private slots:
    void on_solvers_combo_currentIndexChanged(int index);

    void on_updateButton_clicked();

    void on_deleteButton_clicked();

    void on_mznpath_select_clicked();

    void on_exec_select_clicked();

    void on_solver_default_stateChanged(int arg1);

    void on_check_updates_stateChanged(int arg1);

    void on_send_stats_stateChanged(int arg1);

    void on_mznDistribPath_editingFinished();

private:
    Ui::SolverDialog *ui;
    QVector<Solver>& solvers;
    int defaultSolver;
    void editingFinished(bool showError);
};

#endif // SOLVERDIALOG_H
