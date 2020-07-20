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
#include <QJsonObject>

namespace Ui {
class SolverDialog;
}

struct SolverFlag {
    QString name;
    QString description;
    enum { T_INT, T_INT_RANGE, T_BOOL, T_BOOL_ONOFF, T_FLOAT, T_FLOAT_RANGE, T_STRING, T_OPT, T_SOLVER } t;
    double min;
    double max;
    QStringList options;
    QString def;
};

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
    QList<SolverFlag> extraFlags;
    QStringList requiredFlags;
    QStringList defaultFlags;
    QStringList tags;
    QJsonObject json;
    bool isDefaultSolver;
    Solver(void) {}

    Solver(const QJsonObject& json);
    static Solver lookup(const QString& str);

    bool operator==(const Solver&) const;
};

class SolverDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SolverDialog(bool openAsAddNew, QWidget *parent = nullptr);
    ~SolverDialog();
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

    void on_checkSolutions_checkBox_stateChanged(int arg1);

    void on_clearOutput_checkBox_stateChanged(int arg1);

    void on_compressSolutions_checkBox_stateChanged(int arg1);

    void on_compressSolutions_spinBox_valueChanged(int arg1);

private:
    Ui::SolverDialog *ui;

    void editingFinished(bool showError);
};

#endif // SOLVERDIALOG_H
