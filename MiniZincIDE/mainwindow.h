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
#include <QSet>
#include <QFileSystemWatcher>
#include <QNetworkAccessManager>

#include "codeeditor.h"
#include "solverdialog.h"
#include "help.h"
#include "paramdialog.h"
#include "project.h"
#include "htmlwindow.h"

namespace Ui {
class MainWindow;
}

class FindDialog;
class MainWindow;
class QNetworkReply;
class QTextStream;

class IDEStatistics {
public:
    int errorsShown;
    int errorsClicked;
    int modelsRun;
    QStringList solvers;
    QVariantMap toVariantMap(void);
    IDEStatistics(void);
    void init(QVariant v);
    QByteArray toJson(void);
    void resetCounts(void);
};

class IDE : public QApplication {
    Q_OBJECT
public:
    IDE(int& argc, char* argv[]);
    ~IDE(void);
    struct Doc;
    typedef QMap<QString,Doc*> DMap;
    DMap documents;
    typedef QMap<QString,MainWindow*> PMap;
    PMap projects;
    QSet<MainWindow*> mainWindows;

    QStringList recentFiles;
    QStringList recentProjects;

    IDEStatistics stats;

    MainWindow* lastDefaultProject;
    Help* helpWindow;

    QNetworkAccessManager* networkManager;
    QNetworkReply* versionCheckReply;

#ifdef Q_OS_MAC
    QMenuBar* defaultMenuBar;
#endif

    QFileSystemWatcher fsWatch;

    bool hasFile(const QString& path);
    QPair<QTextDocument*,bool> loadFile(const QString& path, QWidget* parent);
    void loadLargeFile(const QString& path, QWidget* parent);
    QTextDocument* addDocument(const QString& path, QTextDocument* doc, CodeEditor* ce);
    void registerEditor(const QString& path, CodeEditor* ce);
    void removeEditor(const QString& path, CodeEditor* ce);
    void renameFile(const QString& oldPath, const QString& newPath);
    QString appDir(void) const;
    static IDE* instance(void);
    QString getLastPath(void);
    void setLastPath(const QString& path);
protected:
    bool event(QEvent *);
protected slots:
    void versionCheckFinished(void);
    void newProject(void);
    void openFile(void);
    void fileModified(const QString&);
public slots:
    void checkUpdate(void);
    void help(void);
};

class MainWindow : public QMainWindow
{
    Q_OBJECT
    friend class IDE;
public:
    explicit MainWindow(const QString& project = QString());
    explicit MainWindow(const QStringList& files);
    ~MainWindow();

private:
    void init(const QString& project);

signals:
    /// emitted when compilation and running of a model has finished
    void finished();

public slots:

    void openFile(const QString &path = QString(), bool openAsModified=false);
    void on_actionStop_triggered();

private slots:

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

    void procFinished(int, bool showTime=true);

    void procError(QProcess::ProcessError);
    void outputProcError(QProcess::ProcessError);

    void on_actionSave_triggered();
    void on_actionQuit_triggered();

    void statusTimerEvent();

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

    void on_actionManage_solvers_triggered(bool addNew=false);

    void on_actionFind_triggered();

    void on_actionReplace_triggered();

    void on_actionSelect_font_triggered();

    void on_actionGo_to_line_triggered();

    void on_actionShift_left_triggered();

    void on_actionShift_right_triggered();

    void on_actionHelp_triggered();

    void on_actionNew_project_triggered();

    void on_actionSave_project_triggered();

    void on_actionSave_project_as_triggered();

    void on_actionClose_project_triggered();

    void on_actionFind_next_triggered();

    void on_actionFind_previous_triggered();

    void on_actionSave_all_triggered();

    void on_action_Un_comment_triggered();

    void on_actionOnly_editor_triggered();

    void on_actionSplit_triggered();

    void on_actionPrevious_tab_triggered();

    void on_actionNext_tab_triggered();

    void recentFileMenuAction(QAction*);

    void recentProjectMenuAction(QAction*);

    void on_actionHide_tool_bar_triggered();

    void on_actionShow_project_explorer_triggered();

    void activateFileInProject(const QModelIndex&);

    void onProjectCustomContextMenu(const QPoint&);

    void onActionProjectOpen_triggered();

    void onActionProjectRemove_triggered();

    void onActionProjectRename_triggered();

    void onActionProjectRunWith_triggered();

    void onActionProjectAdd_triggered();

    void on_actionNewModel_file_triggered();

    void on_actionNewData_file_triggered();

    void on_actionSubmit_to_Coursera_triggered();

    void fileRenamed(const QString&, const QString&);

    void on_conf_timeLimit_valueChanged(int arg1);

    void on_conf_solver_activated(const QString &arg1);

    void onClipboardChanged();

    void on_conf_data_file_activated(const QString &arg1);

    void showWindowMenu(void);
    void windowMenuSelected(QAction*);
    void closeHTMLWindow(void);
protected:
    virtual void closeEvent(QCloseEvent*);
    virtual void dragEnterEvent(QDragEnterEvent *);
    virtual void dropEvent(QDropEvent *);
    bool eventFilter(QObject *, QEvent *);
    void openJSONViewer(void);
    void finishJSONViewer(void);
    void compileAndRun(const QString& modelPath, const QString& additionalCmdlineParams, const QString& additionalDataFile);
public:
    bool runWithOutput(const QString& modelFile, const QString& dataFile, int timeout, QTextStream& outstream);
private:
    Ui::MainWindow *ui;
    CodeEditor* curEditor;
    QString curFilePath;
    HTMLWindow* curHtmlWindow;
    MznProcess* process;
    QString processName;
    MznProcess* outputProcess;
    bool processWasStopped;
    int curJSONHandler;
    bool inJSONHandler;
    bool hadNonJSONOutput;
    QVector<QStringList> JSONOutput;
    QTimer* timer;
    QTimer* solverTimeout;
    int time;
    QElapsedTimer elapsedTime;
    QLabel* statusLabel;
    QFont editorFont;
    QVector<Solver> solvers;
    QString defaultSolver;
    QString mznDistribPath;
    QString getMznDistribPath(void) const;
    QString currentFznTarget;
    bool runSolns2Out;
    QTemporaryDir* tmpDir;
    QVector<QTemporaryDir*> cleanupTmpDirs;
    QVector<MznProcess*> cleanupProcesses;
    FindDialog* findDialog;
    QString projectPath;
    bool saveBeforeRunning;
    QString compileErrors;
    ParamDialog* paramDialog;
    bool compileOnly;
    QString mzn2fzn_executable;
    Project project;
    QMenu* projectContextMenu;
    QAction* projectOpen;
    QAction* projectRemove;
    QAction* projectRename;
    QAction* projectRunWith;
    QAction* projectAdd;
    QString projectSelectedFile;
    QModelIndex projectSelectedIndex;
    int newFileCounter;
    QAction* fakeRunAction;
    QAction* fakeStopAction;
    QAction* fakeCompileAction;
    QAction* minimizeAction;
    QTextStream* outputBuffer;

    void createEditor(const QString& path, bool openAsModified, bool isNewFile);
    QStringList parseConf(bool compileOnly);
    void saveFile(CodeEditor* ce, const QString& filepath);
    void saveProject(const QString& filepath);
    void loadProject(const QString& filepath);
    void setEditorFont(QFont font);
    void setLastPath(const QString& s);
    QString getLastPath(void);
    QString setElapsedTime();
    void setupDznMenu();
    void checkMznPath();
    void updateRecentProjects(const QString& p);
    void updateRecentFiles(const QString& p);
    void addFileToProject(bool dznOnly);
public:
    void addOutput(const QString& s, bool html=true);
    void openProject(const QString& fileName);
    bool isEmptyProject(void);
    void selectJSONSolution(HTMLPage* source, int n);
    const Project& getProject(void) { return project; }
};

#endif // MAINWINDOW_H
