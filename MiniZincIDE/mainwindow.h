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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QFile>
#include <QProcess>
#include <QTimer>
#include <QLabel>
#include <QWebView>
#include <QSet>
#include <QTemporaryDir>
#include <QElapsedTimer>
#include <QApplication>
#include <QMap>

#include "codeeditor.h"
#include "solverdialog.h"
#include "help.h"
#include "paramdialog.h"

namespace Ui {
class MainWindow;
}

class FindDialog;
class MainWindow;

class IDE : public QApplication {
    Q_OBJECT
public:
    IDE(int& argc, char* argv[]);
    struct Doc;
    typedef QMap<QString,Doc*> DMap;
    DMap documents;
    typedef QMap<QString,MainWindow*> PMap;
    PMap projects;

    bool hasFile(const QString& path);
    QPair<QTextDocument*,bool> loadFile(const QString& path, QWidget* parent);
    void loadLargeFile(const QString& path, QWidget* parent);
    QTextDocument* addDocument(const QString& path, QTextDocument* doc, CodeEditor* ce);
    void registerEditor(const QString& path, CodeEditor* ce);
    void removeEditor(const QString& path, CodeEditor* ce);
protected:
    bool event(QEvent *);
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(const QString& project = QString(), QWidget *parent = 0);
    explicit MainWindow(const QStringList& files, QWidget *parent = 0);
    ~MainWindow();

private:
    void init(const QString& project);

private slots:
    void on_actionNew_triggered();

    void openFile(const QString &path = QString(), bool openAsModified=false);

    void on_actionClose_triggered();

    void on_actionOpen_triggered();

    void tabCloseRequest(int);

    void tabChange(int);

    void on_actionRun_triggered();

    void checkArgs(QString filepath);
    void checkArgsOutput();
    void checkArgsFinished(int exitcode);

    void readOutput();

    void pipeOutput();

    void procFinished(int);

    void procError(QProcess::ProcessError);
    void outputProcError(QProcess::ProcessError);

    void on_actionSave_triggered();
    void on_actionQuit_triggered();

    void statusTimerEvent();

    void on_actionStop_triggered();

    void on_actionCompile_triggered();

    void openCompiledFzn(int);

    void runCompiledFzn(int);

    void on_actionSave_as_triggered();

    void on_actionClear_output_triggered();

    void on_actionBigger_font_triggered();

    void on_actionSmaller_font_triggered();

    void on_actionAbout_MiniZinc_IDE_triggered();

    void errorClicked(const QUrl&);

    void on_actionDefault_font_size_triggered();

    void on_actionManage_solvers_triggered();

    void on_actionFind_triggered();

    void on_actionReplace_triggered();

    void on_actionSelect_font_triggered();

    void on_actionGo_to_line_triggered();

    void on_actionOnly_editor_triggered();

    void on_actionOnly_output_triggered();

    void on_actionSplit_triggered();

    void on_actionShift_left_triggered();

    void on_actionShift_right_triggered();

    void on_actionHelp_triggered();

    void on_actionNew_project_triggered();

    void on_actionOpen_project_triggered();

    void on_actionSave_project_triggered();

    void on_actionSave_project_as_triggered();

    void on_actionClose_project_triggered();

    void on_actionFind_next_triggered();

    void on_actionFind_previous_triggered();

    void on_actionSave_all_triggered();

    void on_action_Un_comment_triggered();

protected:
    virtual void closeEvent(QCloseEvent*);
private:
    Ui::MainWindow *ui;
    CodeEditor* curEditor;
    QWebView* webView;
    QProcess* process;
    QString processName;
    QProcess* outputProcess;
    QTimer* timer;
    QTimer* solverTimeout;
    int time;
    QElapsedTimer elapsedTime;
    QLabel* statusLabel;
    QFont editorFont;
    QVector<Solver> solvers;
    QString defaultSolver;
    QString mznDistribPath;
    QString currentFznTarget;
    QTemporaryDir* tmpDir;
    QVector<QTemporaryDir*> cleanupTmpDirs;
    FindDialog* findDialog;
    Help* helpWindow;
    QString projectPath;
    QString lastPath;
    bool saveBeforeRunning;
    QString compileErrors;
    ParamDialog* paramDialog;
    bool compileOnly;

    void createEditor(const QString& path, bool openAsModified);
    QStringList parseConf(bool compileOnly);
    void saveFile(CodeEditor* ce, const QString& filepath);
    void saveProject(const QString& filepath);
    void loadProject(const QString& filepath);
    void setEditorFont(QFont font);
    void addOutput(const QString& s, bool html=true);
    void setElapsedTime();
    void setupDznMenu();
    void checkMznPath();
    IDE* ide();
public:
    void openProject(const QString& fileName);
};

#endif // MAINWINDOW_H
