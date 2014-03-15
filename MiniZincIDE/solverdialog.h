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

class SolverDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SolverDialog(QVector<Solver>& solvers, const QString& def,
                          const QString& mznPath, QWidget *parent = 0);
    ~SolverDialog();
    QString mznPath();
    QString def();
private slots:
    void on_solvers_combo_currentIndexChanged(int index);

    void on_updateButton_clicked();

    void on_deleteButton_clicked();

    void on_mznpath_select_clicked();

    void on_exec_select_clicked();

    void on_solver_default_stateChanged(int arg1);

    void on_check_updates_stateChanged(int arg1);

private:
    Ui::SolverDialog *ui;
    QVector<Solver>& solvers;
    int defaultSolver;
};

#endif // SOLVERDIALOG_H
