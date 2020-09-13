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
#ifdef MINIZINC_IDE_HAVE_WEBENGINE
#include <QWebEngineView>
#else
#include <QWebView>
#endif
#include <QSet>
#include <QTemporaryDir>
#include <QMap>
#include <QSet>
#include <QSortFilterProxyModel>
#include <QToolButton>
#include <QProgressBar>
#include <QComboBox>
#include <QVersionNumber>

#include "codeeditor.h"
#include "solverdialog.h"
#include "paramdialog.h"
#include "project.h"
#include "htmlwindow.h"
#include "moocsubmission.h"
#include "solverconfiguration.h"
#include "process.h"
#include "codechecker.h"

#include "../cp-profiler/src/cpprofiler/conductor.hh"
#include "../cp-profiler/src/cpprofiler/execution_window.hh"
#include "../cp-profiler/src/cpprofiler/analysis/merge_window.hh"

namespace Ui {
class MainWindow;
}

class FindDialog;
class MainWindow;
class QNetworkReply;
class QTextStream;

class MainWindow : public QMainWindow
{
    Q_OBJECT
    friend class IDE;
public:
    explicit MainWindow(const QString& project = QString());
    explicit MainWindow(const QStringList& files);
    ~MainWindow();

private:
    enum CompileMode { CM_RUN, CM_COMPILE, CM_PROFILE };

    void init(const QString& project);
    void compileOrRun(
            CompileMode cm,
            const SolverConfiguration* sc = nullptr,
            const QString& model = QString(),
            const QStringList& data = QStringList(),
            const QString& checker = QString(),
            const QStringList& extraArgs = QStringList());
    bool getModelParameters(const SolverConfiguration& sc, const QString& model, QStringList& data, QStringList& extraArgs);
    QString currentModelFile(void);
    bool promptSaveModified(void);
    void setEditorMenuItemsEnabled(bool enabled);
signals:
    /// emitted when compilation and running of a model has finished
    void finished();

public slots:

    void openFile(const QString &path = QString(), bool openAsModified=false, bool focus=true);
    void stop();

private slots:

    void on_actionClose_triggered();

    void on_actionOpen_triggered();

    void tabCloseRequest(int);

    void tabChange(int);

    void on_actionRun_triggered();

    void procFinished(int, qint64 time);
    void procFinished(int);

    void on_actionSave_triggered();
    void on_actionQuit_triggered();

    void statusTimerEvent(qint64 time);

    void on_actionCompile_triggered();

    void openCompiledFzn(const QString& file);

    void profileCompiledFzn(const QString& output);

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

    void on_actionSave_solver_configuration_triggered();

    void on_actionSave_all_solver_configurations_triggered();

    void on_action_Un_comment_triggered();

    void on_actionOnly_editor_triggered();

    void on_actionSplit_triggered();

    void on_actionPrevious_tab_triggered();

    void on_actionNext_tab_triggered();

    void recentFileMenuAction(QAction*);

    void recentProjectMenuAction(QAction*);

    void on_actionHide_tool_bar_triggered();

    void on_actionToggle_profiler_info_triggered();

    void on_actionShow_project_explorer_triggered();

    void on_actionNewModel_file_triggered();

    void on_actionNewData_file_triggered();

    void on_actionSubmit_to_MOOC_triggered();

    void fileRenamed(const QString&, const QString&);

    void onClipboardChanged();

    void editor_cursor_position_changed();

    void showWindowMenu(void);
    void windowMenuSelected(QAction*);
    void closeHTMLWindow(int);
    void on_actionCheat_Sheet_triggered();
    void check_code();

    void on_actionDark_mode_toggled(bool arg1);

    void moocFinished(int);

    void on_actionEditSolverConfig_triggered();

    void on_b_next_clicked();

    void on_b_prev_clicked();

    void on_b_replacefind_clicked();

    void on_b_replace_clicked();

    void on_b_replaceall_clicked();

    void on_closeFindWidget_clicked();

    void on_find_textEdited(const QString &arg1);

    void on_actionProfile_compilation_triggered();

    void on_configWindow_dockWidget_visibilityChanged(bool visible);

    void on_config_window_itemsChanged(const QStringList& items);

    void on_config_window_selectedIndexChanged(int );

    void on_moocChanged(const MOOCAssignment* mooc);

    void on_projectBrowser_runRequested(const QStringList& files);

    void on_projectBrowser_openRequested(const QStringList& files);

    void on_projectBrowser_removeRequested(const QStringList& files);

    void on_actionShow_search_profiler_triggered();

    void on_actionProfile_search_triggered();

    void showExecutionWindow(cpprofiler::ExecutionWindow& e);
    void showMergeWindow(cpprofiler::analysis::MergeWindow& m);

protected:
    virtual void closeEvent(QCloseEvent*);
    virtual void dragEnterEvent(QDragEnterEvent *);
    virtual void dropEvent(QDropEvent *);
#ifdef Q_OS_MAC
    virtual void paintEvent(QPaintEvent *);
#endif
    bool eventFilter(QObject *, QEvent *);
    void compileAndRun(const QString& modelPath, const QString& additionalCmdlineParams, const QStringList& additionalDataFiles, const QStringList& additionalMznParams);
public:
    void resolve(int htmlWindowIdentifier, const QString &data);
    QString currentSolver(void) const;
    QString currentSolverConfigName(void);
    int addHtmlWindow(HTMLWindow* w);
    bool checkSolutions(void) const;
    void setCheckSolutions(bool b);
    SolverConfiguration* getCurrentSolverConfig(void);
    const Solver* getCurrentSolver(void);
    void compile(const SolverConfiguration& sc, const QString& model, const QStringList& data = QStringList(), const QStringList& extraArgs = QStringList(), bool profile = false);
    void run(const SolverConfiguration& sc, const QString& model, const QStringList& data = QStringList(), const QStringList& extraArgs = QStringList(), QTextStream* ts = nullptr);
private:
    Ui::MainWindow *ui;
    CodeEditor* curEditor;
    QString curFilePath;
    int curHtmlWindow;
    QVector<HTMLWindow*> htmlWindows;
    QVector<QString> htmlWindowModels;
    CodeChecker* code_checker;
    CodeEditor* curCheckEditor;
    int solutionCount;
    int solutionLimit;
    QTimer solutionLimitTimer;
    QVector<QString> hiddenSolutions;
    QProgressBar* progressBar;
    QLabel* statusLabel;
    QLabel* statusLineColLabel;
    QFont editorFont;
    bool darkMode;
    QVector<Solver> solvers;
    QStringList currentAdditionalDataFiles;
    QTemporaryDir* tmpDir;
    QVector<QTemporaryDir*> cleanupTmpDirs;
    QVector<Process*> cleanupProcesses;
    QTextCursor incrementalFindCursor;
    QString projectPath;
    bool saveBeforeRunning;
    ParamDialog* paramDialog;
    bool profileInfoVisible;
    int runTimeout;
    Project* project;
    int newFileCounter;
    QComboBox* solverConfCombo;
    QAction* fakeRunAction;
    QAction* fakeStopAction;
    QAction* fakeCompileAction;
    QAction* minimizeAction;
    QTextStream* outputBuffer;
    MOOCSubmission* moocSubmission;
    bool processRunning;

    QToolButton* runButton;
    QVector<QPair<SolverFlag,QWidget*>> extraSolverFlags;
    int currentSolverConfig;
    bool renamingSolverConf;

    cpprofiler::Conductor* conductor = nullptr;
    int ex_id;

    void createEditor(const QString& path, bool openAsModified, bool isNewFile, bool readOnly=false, bool focus=true);
    enum ConfMode { CONF_CHECKARGS, CONF_COMPILE, CONF_RUN };
    QStringList parseConf(const ConfMode& confMode, const QString& modelFile);
    void saveFile(CodeEditor* ce, const QString& filepath);
    void saveProject(const QString& filepath);
    void loadProject(const QString& filepath);
    void setEditorFont(QFont font);
    void setLastPath(const QString& s);
    QString getLastPath(void);
    QString setElapsedTime(qint64 elapsed_t);
    void checkMznPath(const QString& mznPath);
    void updateRecentProjects(const QString& p);
    void updateRecentFiles(const QString& p);
    void updateUiProcessRunning(bool pr);
    void highlightPath(QString& path, int index);
    QVector<CodeEditor*> collectCodeEditors(QVector<QStringList>& locs);
    void updateSolverConfigs(void);
    void setCurrentSolverConfig(int idx);
    void find(bool fwd, bool forceNoWrapAround=false);
    bool requireMiniZinc(void);
    void outputStdErr(const QString& line);
    QStringList getOpenFiles(void);

    void openJSONViewer(bool isJSONinitHandler, const QVector<MznProcess::VisOutput>& output);
    void finishJSONViewer(qint64 time);
public:
    void addOutput(const QString& s, bool html=true);
    void openProject(const QString& fileName);
    bool isEmptyProject(void);
    void selectJSONSolution(HTMLPage* source, int n);
    Project& getProject(void) { return *project; }
};

#endif // MAINWINDOW_H
