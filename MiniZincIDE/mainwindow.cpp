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

#include <QtWidgets>
#include <QApplication>
#include <QSet>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QBoxLayout>
#include <QTemporaryFile>
#include <csignal>
#include <sstream>

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "ide.h"
#include "codeeditor.h"
#include "fzndoc.h"
#include "gotolinedialog.h"
#include "paramdialog.h"
#include "checkupdatedialog.h"
#include "moocsubmission.h"
#include "highlighter.h"
#include "exception.h"

#include <QtGlobal>

#define BUILD_YEAR  (__DATE__ + 7)

MainWindow::MainWindow(const QString& project) :
    ui(new Ui::MainWindow),
    curEditor(nullptr),
    curHtmlWindow(-1),
    compileProcess(nullptr),
    solveProcess(nullptr),
    code_checker(nullptr),
    tmpDir(nullptr),
    saveBeforeRunning(false),
    outputBuffer(nullptr),
    processRunning(false),
    currentSolverConfig(-1)
{
    init(project);
}

MainWindow::MainWindow(const QStringList& files) :
    ui(new Ui::MainWindow),
    curEditor(nullptr),
    curHtmlWindow(-1),
    compileProcess(nullptr),
    solveProcess(nullptr),
    code_checker(nullptr),
    tmpDir(nullptr),
    saveBeforeRunning(false),
    outputBuffer(nullptr),
    processRunning(false),
    currentSolverConfig(-1)
{
    init(QString());
    for (int i=0; i<files.size(); i++)
        openFile(files[i],false);

}

void MainWindow::showWindowMenu()
{
    ui->menuWindow->clear();
    ui->menuWindow->addAction(minimizeAction);
    ui->menuWindow->addSeparator();
    for (QSet<MainWindow*>::iterator it = IDE::instance()->mainWindows.begin();
         it != IDE::instance()->mainWindows.end(); ++it) {
        QAction* windowAction = ui->menuWindow->addAction((*it)->windowTitle());
        QVariant v = qVariantFromValue(static_cast<void*>(*it));
        windowAction->setData(v);
        windowAction->setCheckable(true);
        if (*it == this) {
            windowAction->setChecked(true);
        }
    }
}

void MainWindow::windowMenuSelected(QAction* a)
{
    if (a==minimizeAction) {
        showMinimized();
    } else {
        QMainWindow* mw = static_cast<QMainWindow*>(a->data().value<void*>());
        mw->showNormal();
        mw->raise();
        mw->activateWindow();
    }
}

void MainWindow::init(const QString& projectFile)
{
    IDE::instance()->mainWindows.insert(this);
    ui->setupUi(this);
    ui->tabWidget->removeTab(0);
#ifndef Q_OS_MAC
    ui->menuFile->addAction(ui->actionQuit);
#else
    if (hasDarkMode()) {
        ui->menuView->removeAction(ui->actionDark_mode);
    }
#endif

    // initialise find widget
    ui->findFrame->hide();
    connect(ui->find, SIGNAL(escPressed(void)), this, SLOT(on_closeFindWidget_clicked()));


    QWidget* solverConfFrame = new QWidget;
    QVBoxLayout* solverConfFrameLayout = new QVBoxLayout;

    solverConfCombo = new QComboBox;
    solverConfCombo->setMinimumWidth(100);
    QLabel* solverConfLabel = new QLabel("Solver configuration:");
    QFont solverConfLabelFont = solverConfLabel->font();
    solverConfLabelFont.setPointSizeF(solverConfLabelFont.pointSizeF()*0.9);
    solverConfLabel->setFont(solverConfLabelFont);
    solverConfFrameLayout->addWidget(solverConfLabel);
    solverConfFrameLayout->addWidget(solverConfCombo);
    solverConfFrame->setLayout(solverConfFrameLayout);
    QAction* solverConfComboAction = ui->toolBar->insertWidget(ui->actionSubmit_to_MOOC, solverConfFrame);

    runButton = new QToolButton;
    runButton->setDefaultAction(ui->actionRun);
    runButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    ui->toolBar->insertWidget(solverConfComboAction, runButton);

    ui->outputConsole->installEventFilter(this);
    setAcceptDrops(true);
    setAttribute(Qt::WA_DeleteOnClose, true);
    minimizeAction = new QAction("&Minimize",this);
    minimizeAction->setShortcut(Qt::CTRL+Qt::Key_M);
#ifdef Q_OS_MAC
    connect(ui->menuWindow, SIGNAL(aboutToShow()), this, SLOT(showWindowMenu()));
    connect(ui->menuWindow, SIGNAL(triggered(QAction*)), this, SLOT(windowMenuSelected(QAction*)));
    ui->menuWindow->addAction(minimizeAction);
    ui->menuWindow->addSeparator();
#else
    ui->menuWindow->hide();
    ui->menubar->removeAction(ui->menuWindow->menuAction());
#endif
    QWidget* toolBarSpacer = new QWidget();
    toolBarSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    ui->toolBar->insertWidget(ui->actionEditSolverConfig, toolBarSpacer);

    newFileCounter = 1;

    profileInfoVisible = false;

    paramDialog = new ParamDialog(this);

    fakeRunAction = new QAction(this);
    fakeRunAction->setShortcut(Qt::CTRL+Qt::Key_R);
    fakeRunAction->setEnabled(true);
    this->addAction(fakeRunAction);

    fakeCompileAction = new QAction(this);
    fakeCompileAction->setShortcut(Qt::CTRL+Qt::Key_B);
    fakeCompileAction->setEnabled(true);
    this->addAction(fakeCompileAction);

    fakeStopAction = new QAction(this);
    fakeStopAction->setShortcut(Qt::CTRL+Qt::Key_E);
    fakeStopAction->setEnabled(true);
    this->addAction(fakeStopAction);

    updateRecentProjects("");
    updateRecentFiles("");
    connect(ui->menuRecent_Files, SIGNAL(triggered(QAction*)), this, SLOT(recentFileMenuAction(QAction*)));
    connect(ui->menuRecent_Projects, SIGNAL(triggered(QAction*)), this, SLOT(recentProjectMenuAction(QAction*)));

    connect(ui->tabWidget, SIGNAL(tabCloseRequested(int)), this, SLOT(tabCloseRequest(int)));
    connect(ui->tabWidget, SIGNAL(currentChanged(int)), this, SLOT(tabChange(int)));
    timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(statusTimerEvent()));
    solverTimeout = new QTimer(this);
    solverTimeout->setSingleShot(true);
    connect(solverTimeout, SIGNAL(timeout()), this, SLOT(on_actionStop_triggered()));

    progressBar = new QProgressBar;
    progressBar->setRange(0, 100);
    progressBar->setSizePolicy(QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Minimum);
    progressBar->setHidden(true);
    statusLabel = new QLabel("");
    statusLineColLabel = new QLabel("");
    ui->statusbar->addPermanentWidget(statusLabel);
    ui->statusbar->addPermanentWidget(progressBar);
    ui->statusbar->addWidget(statusLineColLabel);
    ui->actionStop->setEnabled(false);
    QTabBar* tb = ui->tabWidget->findChild<QTabBar*>();
    tb->setTabButton(0, QTabBar::RightSide, 0);
    tb->setTabButton(0, QTabBar::LeftSide, 0);

    ui->actionSubmit_to_MOOC->setVisible(false);

    connect(ui->outputConsole, SIGNAL(anchorClicked(QUrl)), this, SLOT(errorClicked(QUrl)));

    QSettings settings;
    settings.beginGroup("MainWindow");

    QFont defaultFont;
    defaultFont.setFamily("Menlo");
    if (!defaultFont.exactMatch()) {
        defaultFont.setFamily("Consolas");
    }
    if (!defaultFont.exactMatch()) {
        defaultFont.setFamily("Courier New");
    }
    defaultFont.setStyleHint(QFont::TypeWriter);
    defaultFont.setPointSize(13);
    editorFont.fromString(settings.value("editorFont", defaultFont.toString()).value<QString>());
    darkMode = settings.value("darkMode", false).value<bool>();
    ui->actionDark_mode->setChecked(darkMode);
    on_actionDark_mode_toggled(darkMode);
    ui->outputConsole->setFont(editorFont);
    resize(settings.value("size", QSize(800, 600)).toSize());
    move(settings.value("pos", QPoint(100, 100)).toPoint());
    if (settings.value("toolbarHidden", false).toBool()) {
        on_actionHide_tool_bar_triggered();
    }
    if (settings.value("outputWindowHidden", true).toBool()) {
        on_actionOnly_editor_triggered();
    }
    ui->check_wrap->setChecked(settings.value("findWrapAround", false).toBool());
    ui->check_re->setChecked(settings.value("findRegularExpression", false).toBool());
    ui->check_case->setChecked(settings.value("findCaseSensitive", false).toBool());
    settings.endGroup();

    IDE::instance()->setEditorFont(editorFont);

    settings.beginGroup("minizinc");
    QString mznDistribPath = settings.value("mznpath","").toString();
    settings.endGroup();
    checkMznPath(mznDistribPath);

    connect(QApplication::clipboard(), SIGNAL(dataChanged()), this, SLOT(onClipboardChanged()));

    projectSort = new QSortFilterProxyModel(this);
    projectSort->setDynamicSortFilter(true);
    projectSort->setSourceModel(&project);
    projectSort->setSortRole(Qt::UserRole);
    ui->projectView->setModel(projectSort);
    ui->projectView->sortByColumn(0, Qt::AscendingOrder);
    ui->projectView->setEditTriggers(QAbstractItemView::EditKeyPressed);
    ui->projectExplorerDockWidget->hide();
//    ui->conf_dock_widget->hide();
    connect(ui->projectView, SIGNAL(activated(QModelIndex)),
            this, SLOT(activateFileInProject(QModelIndex)));

    projectContextMenu = new QMenu(ui->projectView);
    projectOpen = projectContextMenu->addAction("Open file", this, SLOT(onActionProjectOpen_triggered()));
    projectRemove = projectContextMenu->addAction("Remove from project", this, SLOT(onActionProjectRemove_triggered()));
    projectRename = projectContextMenu->addAction("Rename file", this, SLOT(onActionProjectRename_triggered()));
    projectRunWith = projectContextMenu->addAction("Run model with this data", this, SLOT(onActionProjectRunWith_triggered()));
    projectAdd = projectContextMenu->addAction("Add existing file...", this, SLOT(onActionProjectAdd_triggered()));

    ui->projectView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->projectView, SIGNAL(customContextMenuRequested(QPoint)),
            this, SLOT(onProjectCustomContextMenu(QPoint)));
    connect(&project, SIGNAL(fileRenamed(QString,QString)), this, SLOT(fileRenamed(QString,QString)));
    connect(&project, &Project::moocButtonChanged, this, &MainWindow::on_moocButtonChanged);

    ui->actionNext_tab->setShortcuts(QKeySequence::NextChild);
    ui->actionPrevious_tab->setShortcuts(QKeySequence::PreviousChild);

    if (!projectFile.isEmpty()) {
        loadProject(projectFile);
        setLastPath(QFileInfo(projectFile).absolutePath()+fileDialogSuffix);
    } else {
        createEditor("Playground",false,true);
        if (getLastPath().isEmpty()) {
            setLastPath(QDir::currentPath()+fileDialogSuffix);
        }
    }
}

void MainWindow::onProjectCustomContextMenu(const QPoint & point)
{
    projectSelectedIndex = projectSort->mapToSource(ui->projectView->indexAt(point));
    QString file = project.fileAtIndex(projectSelectedIndex);
    if (!file.isEmpty()) {
        projectSelectedFile = file;
        projectOpen->setEnabled(true);
        projectRemove->setEnabled(true);
        projectRename->setEnabled(true);
        projectRunWith->setEnabled(ui->actionRun->isEnabled() && (file.endsWith(".dzn") || file.endsWith(".json")));
        projectContextMenu->exec(ui->projectView->mapToGlobal(point));
    } else {
        projectOpen->setEnabled(false);
        projectRemove->setEnabled(false);
        projectRename->setEnabled(false);
        projectRunWith->setEnabled(false);
        projectContextMenu->exec(ui->projectView->mapToGlobal(point));
    }
}

void MainWindow::onActionProjectAdd_triggered()
{
    addFileToProject(false);
}

void MainWindow::addFileToProject(bool dznOnly)
{
    QStringList fileNames;
    if (dznOnly) {
        QString fileName = QFileDialog::getOpenFileName(this, tr("Select a data file to open"), getLastPath(), "MiniZinc data files (*.dzn *.json)");
        if (!fileName.isNull())
            fileNames.append(fileName);
    } else {
        fileNames = QFileDialog::getOpenFileNames(this, tr("Select one or more files to open"), getLastPath(), "MiniZinc Files (*.mzn *.dzn *.json *.mpc)");
    }

    for (QStringList::iterator it = fileNames.begin(); it != fileNames.end(); ++it) {
        setLastPath(QFileInfo(*it).absolutePath()+fileDialogSuffix);
        project.addFile(ui->projectView, projectSort, *it);
    }
}

void MainWindow::updateUiProcessRunning(bool pr)
{
    processRunning = pr;

    if (processRunning) {
        fakeRunAction->setEnabled(true);
        ui->actionRun->setEnabled(false);
        ui->actionProfile_compilation->setEnabled(false);
        fakeCompileAction->setEnabled(true);
        ui->actionCompile->setEnabled(false);
        fakeStopAction->setEnabled(false);
        ui->actionStop->setEnabled(true);
        runButton->removeAction(ui->actionRun);
        runButton->setDefaultAction(ui->actionStop);
        ui->actionSubmit_to_MOOC->setEnabled(false);
    } else {
        bool isPlayground = false;
        bool isMzn = false;
        bool isFzn = false;
        bool isData = false;
        if (curEditor) {
            isPlayground = curEditor->filepath=="";
            isMzn = isPlayground || QFileInfo(curEditor->filepath).suffix()=="mzn";
            isFzn = QFileInfo(curEditor->filepath).suffix()=="fzn";
            isData = QFileInfo(curEditor->filepath).suffix()=="dzn" || QFileInfo(curEditor->filepath).suffix()=="json";
        }
        fakeRunAction->setEnabled(! (isMzn || isFzn || isData));
        ui->actionRun->setEnabled(isMzn || isFzn || isData);
        ui->actionProfile_compilation->setEnabled((!isPlayground && isMzn) || isData);
        fakeCompileAction->setEnabled(!(isMzn||isData));
        ui->actionCompile->setEnabled(isMzn||isData);
        fakeStopAction->setEnabled(true);
        ui->actionStop->setEnabled(false);
        runButton->removeAction(ui->actionStop);
        runButton->setDefaultAction(ui->actionRun);
        ui->actionSubmit_to_MOOC->setEnabled(true);
    }
}

void MainWindow::onActionProjectOpen_triggered()
{
    activateFileInProject(projectSelectedIndex);
}

void MainWindow::onActionProjectRemove_triggered()
{
    int tabCount = ui->tabWidget->count();
    if (!projectSelectedFile.isEmpty()) {
        for (int i=0; i<tabCount; i++) {
            CodeEditor* ce = static_cast<CodeEditor*>(ui->tabWidget->widget(i));
            if (ce->filepath == projectSelectedFile) {
                tabCloseRequest(i);
                if (ui->tabWidget->count() == tabCount)
                    return;
                tabCount = ui->tabWidget->count();
                i--;
            }
        }
    }
    project.removeFile(projectSelectedFile);
    tabChange(ui->tabWidget->currentIndex());
}

void MainWindow::onActionProjectRename_triggered()
{
    ui->projectView->edit(ui->projectView->currentIndex());
}

void MainWindow::onActionProjectRunWith_triggered()
{
    currentAdditionalDataFiles.clear();
    currentAdditionalDataFiles.append(projectSelectedFile);
    on_actionRun_triggered();
}

void MainWindow::activateFileInProject(const QModelIndex &proxyIndex)
{
    QModelIndex index = projectSort->mapToSource(proxyIndex);
    if (project.isProjectFile(index) && ui->configWindow_dockWidget->isHidden()) {
//        on_actionEditSolverConfig_triggered();
    } else {
        QString fileName = project.fileAtIndex(index);
        if (!fileName.isEmpty()) {
            if (fileName.endsWith(".mpc")) {
                int i = 0;
                for (auto& sc : ui->config_window->solverConfigs()) {
                    if (sc.paramFile == fileName) {
                        ui->config_window->setCurrentIndex(i);
                        ui->configWindow_dockWidget->setVisible(true);
                        break;
                    }
                    i++;
                }
                return;
            }
            bool foundFile = false;
            for (int i=0; i<ui->tabWidget->count(); i++) {
                CodeEditor* ce = static_cast<CodeEditor*>(ui->tabWidget->widget(i));
                if (ce->filepath == fileName) {
                    ui->tabWidget->setCurrentIndex(i);
                    foundFile = true;
                    break;
                }
            }
            if (!foundFile) {
                createEditor(fileName,false,false);
            }
        }
    }
}

MainWindow::~MainWindow()
{
    for (int i=0; i<cleanupTmpDirs.size(); i++) {
        delete cleanupTmpDirs[i];
    }
    for (int i=0; i<cleanupProcesses.size(); i++) {
        cleanupProcesses[i]->terminate();
        delete cleanupProcesses[i];
    }
    if (solveProcess) {
        solveProcess->terminate();
        solveProcess->deleteLater();
    }
    delete ui;
    delete paramDialog;
}

void MainWindow::on_actionNewModel_file_triggered()
{
    createEditor(".mzn",false,true);
}

void MainWindow::on_actionNewData_file_triggered()
{
    createEditor(".dzn",false,true);
}

void MainWindow::createEditor(const QString& path, bool openAsModified, bool isNewFile, bool readOnly, bool focus) {
    QTextDocument* doc = nullptr;
    bool large = false;
    QString fileContents;
    QString absPath = QFileInfo(path).canonicalFilePath();
    if (isNewFile) {
        if (path=="Playground") {
            absPath=path;
            fileContents="% Use this editor as a MiniZinc scratch book\n";
            openAsModified=false;
        } else {
            absPath = QString("Untitled")+QString().setNum(newFileCounter++)+path;
        }
    } else if (path.isEmpty()) {
        absPath = path;
        // Do nothing
    } else if (openAsModified) {
        QFile file(path);
        if (file.open(QFile::ReadOnly)) {
            fileContents = file.readAll();
        } else {
            QMessageBox::warning(this,"MiniZinc IDE",
                                 "Could not open file "+path,
                                 QMessageBox::Ok);
            return;
        }
    } else {
        if (absPath.isEmpty()) {
            QMessageBox::warning(this,"MiniZinc IDE",
                                 "Could not open file "+path,
                                 QMessageBox::Ok);
            return;
        }
        QPair<QTextDocument*,bool> d = IDE::instance()->loadFile(absPath,this);
        updateRecentFiles(absPath);
        doc = d.first;
        large = d.second;
    }
    if (doc || !fileContents.isEmpty() || isNewFile) {
        int closeTab = -1;
        if (!isNewFile && ui->tabWidget->count()==1) {
            CodeEditor* ce = static_cast<CodeEditor*>(ui->tabWidget->widget(0));
            if (ce->filepath == "" && !ce->document()->isModified()) {
                closeTab = 0;
            }
        }
        CodeEditor* ce = new CodeEditor(doc,absPath,isNewFile,large,editorFont,darkMode,ui->tabWidget,this);
        if (readOnly || ce->filename == "_coursera" || ce->filename.endsWith(".mzc"))
            ce->setReadOnly(true);
        int tab = ui->tabWidget->addTab(ce, ce->filename);
        if(profileInfoVisible)
            ce->showDebugInfo(profileInfoVisible);
        if (focus) {
            ui->tabWidget->setCurrentIndex(tab);
            curEditor->setFocus();
        }
        if (!fileContents.isEmpty()) {
            curEditor->filepath = "";
            curEditor->document()->setPlainText(fileContents);
            curEditor->document()->setModified(openAsModified);
            tabChange(ui->tabWidget->currentIndex());
        } else if (doc) {
            project.addFile(ui->projectView, projectSort, absPath);
            IDE::instance()->registerEditor(absPath,curEditor);
        }
        if (closeTab >= 0)
            tabCloseRequest(closeTab);
    }
}

void MainWindow::setLastPath(const QString &s)
{
    IDE::instance()->setLastPath(s);
}

QString MainWindow::getLastPath(void)
{
    return IDE::instance()->getLastPath();
}

void MainWindow::openFile(const QString &path, bool openAsModified, bool focus)
{
    QString fileName = path;

    if (fileName.isNull()) {
        fileName = QFileDialog::getOpenFileName(this, tr("Open File"), getLastPath(), "MiniZinc Files (*.mzn *.dzn *.fzn *.json *.mzp *.mzc *.mpc);;Other (*)");
        if (!fileName.isNull()) {
            setLastPath(QFileInfo(fileName).absolutePath()+fileDialogSuffix);
        }
    }

    if (!fileName.isEmpty()) {
        if (fileName.endsWith(".mzp")) {
            openProject(fileName);
        } else if (fileName.endsWith(".mpc")) {
            QString absPath = QFileInfo(fileName).canonicalFilePath();
            ui->config_window->addConfig(absPath);
            project.addFile(ui->projectView, projectSort, absPath);
            ui->configWindow_dockWidget->setVisible(true);
        } else {
            createEditor(fileName, openAsModified, false, false, focus);
        }
    }

}

void MainWindow::tabCloseRequest(int tab)
{
    CodeEditor* ce = static_cast<CodeEditor*>(ui->tabWidget->widget(tab));
    if (ce->document()->isModified()) {
        QMessageBox msg;
        msg.setText("The document has been modified.");
        msg.setInformativeText("Do you want to save your changes?");
        msg.setStandardButtons(QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
        msg.setDefaultButton(QMessageBox::Save);
        int ret = msg.exec();
        switch (ret) {
        case QMessageBox::Save:
            saveFile(ce,ce->filepath);
            if (ce->document()->isModified())
                return;
            break;
        case QMessageBox::Discard:
            break;
        case QMessageBox::Cancel:
            return;
        default:
            return;
        }
    }
    ce->document()->setModified(false);
    ui->tabWidget->removeTab(tab);
    if (!ce->filepath.isEmpty())
        IDE::instance()->removeEditor(ce->filepath,ce);
    delete ce;
}

void MainWindow::closeEvent(QCloseEvent* e) {
    // make sure any modifications in solver configurations are saved
//    on_conf_solver_conf_currentIndexChanged(ui->conf_solver_conf->currentIndex());

    bool modified = false;
    for (int i=0; i<ui->tabWidget->count(); i++) {
        if (static_cast<CodeEditor*>(ui->tabWidget->widget(i))->document()->isModified()) {
            modified = true;
            break;
        }
    }
    if (modified) {
        int ret = QMessageBox::warning(this, "MiniZinc IDE",
                                       "There are modified documents.\nDo you want to discard the changes or cancel?",
                                       QMessageBox::Discard | QMessageBox::Cancel);
        if (ret == QMessageBox::Cancel) {
            e->ignore();
            return;
        }
    }
    if (project.isModified()) {
        int ret = QMessageBox::warning(this, "MiniZinc IDE",
                                       "The project has been modified.\nDo you want to discard the changes or cancel?",
                                       QMessageBox::Discard | QMessageBox::Cancel);
        if (ret == QMessageBox::Cancel) {
            e->ignore();
            return;
        }
    }
    if (solveProcess) {
        int ret = QMessageBox::warning(this, "MiniZinc IDE",
                                       "MiniZinc is currently running a solver.\nDo you want to quit anyway and stop the current process?",
                                       QMessageBox::Yes| QMessageBox::No);
        if (ret == QMessageBox::No) {
            e->ignore();
            return;
        }
    }
    if (solveProcess) {
        disconnect(solveProcess, SIGNAL(error(QProcess::ProcessError)),
                   this, 0);
        solveProcess->terminate();
        solveProcess->deleteLater();
        solveProcess = nullptr;
    }
    for (int i=0; i<ui->tabWidget->count(); i++) {
        CodeEditor* ce = static_cast<CodeEditor*>(ui->tabWidget->widget(i));
        ce->setDocument(nullptr);
        ce->filepath = "";
        if (ce->filepath != "")
            IDE::instance()->removeEditor(ce->filepath,ce);
    }
    if (!projectPath.isEmpty())
        IDE::instance()->projects.remove(projectPath);
    projectPath = "";

    IDE::instance()->mainWindows.remove(this);

    QSettings settings;
    settings.beginGroup("MainWindow");
    settings.setValue("editorFont", editorFont.toString());
    settings.setValue("darkMode", darkMode);
    settings.setValue("size", size());
    settings.setValue("pos", pos());
    settings.setValue("toolbarHidden", ui->toolBar->isHidden());
    settings.setValue("outputWindowHidden", ui->outputDockWidget->isHidden());

    settings.setValue("findWrapAround", ui->check_wrap->isChecked());
    settings.setValue("findRegularExpression", ui->check_re->isChecked());
    settings.setValue("findCaseSensitive", ui->check_case->isChecked());

    settings.endGroup();
    e->accept();
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasFormat("text/uri-list"))
        event->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent* event) {
    const QMimeData* mimeData = event->mimeData();
    if (mimeData->hasUrls()) {
        QList<QUrl> urlList = mimeData->urls();
        for (int i=0; i<urlList.size(); i++) {
            openFile(urlList[i].toLocalFile());
        }
    }
    event->acceptProposedAction();
}

#ifdef Q_OS_MAC
void MainWindow::paintEvent(QPaintEvent *) {
    if (hasDarkMode()) {
        bool newDark = isDark();
        if (newDark != darkMode) {
            on_actionDark_mode_toggled(newDark);
        }
    }
}
#endif

void MainWindow::tabChange(int tab) {
    if (curEditor) {
        disconnect(ui->actionCopy, SIGNAL(triggered()), curEditor, SLOT(copy()));
        disconnect(ui->actionPaste, SIGNAL(triggered()), curEditor, SLOT(paste()));
        disconnect(ui->actionCut, SIGNAL(triggered()), curEditor, SLOT(cut()));
        disconnect(ui->actionUndo, SIGNAL(triggered()), curEditor, SLOT(undo()));
        disconnect(ui->actionRedo, SIGNAL(triggered()), curEditor, SLOT(redo()));
        disconnect(curEditor, SIGNAL(copyAvailable(bool)), ui->actionCopy, SLOT(setEnabled(bool)));
        disconnect(curEditor, SIGNAL(copyAvailable(bool)), ui->actionCut, SLOT(setEnabled(bool)));
        disconnect(curEditor->document(), SIGNAL(modificationChanged(bool)),
                   this, SLOT(setWindowModified(bool)));
        disconnect(curEditor->document(), SIGNAL(undoAvailable(bool)),
                   ui->actionUndo, SLOT(setEnabled(bool)));
        disconnect(curEditor->document(), SIGNAL(redoAvailable(bool)),
                   ui->actionRedo, SLOT(setEnabled(bool)));
        disconnect(curEditor, SIGNAL(cursorPositionChanged()), this, SLOT(editor_cursor_position_changed()));
        disconnect(curEditor, &CodeEditor::changedDebounced, this, &MainWindow::check_code);
    }
    if (tab==-1) {
        curEditor = nullptr;
        setEditorMenuItemsEnabled(false);
        ui->findFrame->hide();
    } else {
        setEditorMenuItemsEnabled(true);
        curEditor = static_cast<CodeEditor*>(ui->tabWidget->widget(tab));
        connect(ui->actionCopy, SIGNAL(triggered()), curEditor, SLOT(copy()));
        connect(ui->actionPaste, SIGNAL(triggered()), curEditor, SLOT(paste()));
        connect(ui->actionCut, SIGNAL(triggered()), curEditor, SLOT(cut()));
        connect(ui->actionSelect_All, SIGNAL(triggered()), curEditor, SLOT(selectAll()));
        connect(ui->actionUndo, SIGNAL(triggered()), curEditor, SLOT(undo()));
        connect(ui->actionRedo, SIGNAL(triggered()), curEditor, SLOT(redo()));
        connect(curEditor, SIGNAL(copyAvailable(bool)), ui->actionCopy, SLOT(setEnabled(bool)));
        connect(curEditor, SIGNAL(copyAvailable(bool)), ui->actionCut, SLOT(setEnabled(bool)));
        connect(curEditor->document(), SIGNAL(modificationChanged(bool)),
                this, SLOT(setWindowModified(bool)));
        connect(curEditor->document(), SIGNAL(undoAvailable(bool)),
                ui->actionUndo, SLOT(setEnabled(bool)));
        connect(curEditor->document(), SIGNAL(redoAvailable(bool)),
                ui->actionRedo, SLOT(setEnabled(bool)));
        connect(curEditor, SIGNAL(cursorPositionChanged()), this, SLOT(editor_cursor_position_changed()));
        connect(curEditor, &CodeEditor::changedDebounced, this, &MainWindow::check_code);
        setWindowModified(curEditor->document()->isModified());
        QString p;
        p += " ";
        p += QChar(0x2014);
        p += " ";
        if (projectPath.isEmpty()) {
            p += "Untitled Project";
        } else {
            QFileInfo fi(projectPath);
            p += "Project: "+fi.baseName();
        }
        if (curEditor->filepath.isEmpty()) {
            setWindowFilePath(curEditor->filename);
            setWindowTitle(curEditor->filename+p+"[*]");
        } else {
            setWindowFilePath(curEditor->filepath);
            setWindowTitle(curEditor->filename+p+"[*]");

            bool haveChecker = false;
            if (curEditor->filename.endsWith(".mzn")) {
                QString checkFile = curEditor->filepath;
                checkFile.replace(checkFile.length()-1,1,"c");
                haveChecker = project.containsFile(checkFile) || project.containsFile(checkFile+".mzn");
            }
            QSettings settings;
            settings.beginGroup("ide");
            bool checkSolutions = haveChecker && settings.value("checkSolutions", true).toBool();
            if (checkSolutions) {
                ui->actionRun->setText("Run + check");
            } else {
                ui->actionRun->setText("Run");
            }

        }
        ui->actionSave->setEnabled(true);
        ui->actionSave_as->setEnabled(true);
        ui->actionSelect_All->setEnabled(true);
        ui->actionUndo->setEnabled(curEditor->document()->isUndoAvailable());
        ui->actionRedo->setEnabled(curEditor->document()->isRedoAvailable());
        updateUiProcessRunning(processRunning);

        ui->actionFind->setEnabled(true);
        ui->actionFind_next->setEnabled(true);
        ui->actionFind_previous->setEnabled(true);
        ui->actionReplace->setEnabled(true);
        ui->actionShift_left->setEnabled(true);
        ui->actionShift_right->setEnabled(true);
        curEditor->setFocus();

        MainWindow::check_code();
    }
}

void MainWindow::on_actionClose_triggered()
{
    int tab = ui->tabWidget->currentIndex();
    tabCloseRequest(tab);
}

void MainWindow::on_actionOpen_triggered()
{
    openFile(QString());
}

void MainWindow::addOutput(const QString& s, bool html)
{
    QTextCursor cursor = ui->outputConsole->textCursor();
    cursor.movePosition(QTextCursor::End);
    ui->outputConsole->setTextCursor(cursor);
    if (html) {
        cursor.insertHtml(s+"<br />");
    } else {
        cursor.insertText(s);
    }
}

void MainWindow::on_actionRun_triggered()
{
    curHtmlWindow = -1;
    compileOrRun(CM_RUN);
}

void MainWindow::compileOrRun(CompileMode compileMode)
{
    auto sc = getCurrentSolver();

    if (!requireMiniZinc() || !curEditor || !sc) {
        return;
    }

    // Determine which model needs to be run
    QString filepath;
    QVector<CodeEditor*> modifiedDocs;
    if (!curEditor->filepath.isEmpty()) {
        if (!curEditor->filepath.endsWith(".mzn")) {
            // run a data file, find the model file
            QVector<CodeEditor*> models;
            for (int i=0; i<ui->tabWidget->count(); i++) {
                CodeEditor* ce = static_cast<CodeEditor*>(ui->tabWidget->widget(i));
                if (ce->filepath.endsWith(".mzn")) {
                    models.append(ce);
                }
            }
            int selectedModel = -1;
            if (models.size()==1) {
                selectedModel = 0;
            } else if (models.size() > 1) {
                QStringList modelFiles;
                for (auto ce : models) {
                    modelFiles.append(ce->filepath);
                }
                selectedModel = paramDialog->getModel(modelFiles);
            }
            if (selectedModel==-1)
                return;
            filepath = models[selectedModel]->filepath;
            if (models[selectedModel]->document()->isModified())
                modifiedDocs.append(models[selectedModel]);
            currentAdditionalDataFiles.append(curEditor->filepath);
            if (curEditor->document()->isModified()) {
                modifiedDocs.append(curEditor);
            }
        } else {
            filepath = curEditor->filepath;
            if (curEditor->document()->isModified()) {
                modifiedDocs.append(curEditor);
            }
        }
    } else {
        QTemporaryDir* modelTmpDir = new QTemporaryDir;
        if (!modelTmpDir->isValid()) {
            QMessageBox::critical(this, "MiniZinc IDE", "Could not create temporary directory for compilation.");
        } else {
            cleanupTmpDirs.append(modelTmpDir);
            filepath = modelTmpDir->path()+"/untitled_model.mzn";
            QFile modelFile(filepath);
            if (modelFile.open(QIODevice::ReadWrite)) {
                QTextStream ts(&modelFile);
                ts.setCodec("UTF-8");
                ts << curEditor->document()->toPlainText();
                modelFile.close();
            } else {
                QMessageBox::critical(this, "MiniZinc IDE", "Could not write temporary model file.");
                filepath = "";
            }
        }
    }
    if (filepath.isEmpty()) {
        return;
    }

    // Prompt for saving modified files
    if (!modifiedDocs.empty()) {
        if (!saveBeforeRunning) {
            QMessageBox msgBox;
            if (modifiedDocs.size()==1) {
                msgBox.setText("One of the files is modified. You have to save it before running.");
            } else {
                msgBox.setText("Several files have been modified. You have to save them before running.");
            }
            msgBox.setInformativeText("Do you want to save now and then run?");
            QAbstractButton *saveButton = msgBox.addButton(QMessageBox::Save);
            msgBox.addButton(QMessageBox::Cancel);
            QAbstractButton *alwaysButton = msgBox.addButton("Always save", QMessageBox::AcceptRole);
            msgBox.setDefaultButton(QMessageBox::Save);
            msgBox.exec();
            if (msgBox.clickedButton()==alwaysButton) {
                saveBeforeRunning = true;
            }
            if (msgBox.clickedButton()!=saveButton && msgBox.clickedButton()!=alwaysButton) {
                currentAdditionalDataFiles.clear();
                return;
            }
        }
        for (auto ce : modifiedDocs) {
            saveFile(ce,ce->filepath);
            if (ce->document()->isModified()) {
                currentAdditionalDataFiles.clear();
                return;
            }
        }
    }

    QSettings settings;
    settings.beginGroup("ide");
    bool autoClear = settings.value("clearOutput").toBool();
    settings.endGroup();
    if (autoClear) {
        on_actionClear_output_triggered();
    }

    updateUiProcessRunning(true);
    if (compileMode==CM_RUN) {
        on_actionSplit_triggered();
        IDE::instance()->stats.modelsRun++;
        if (filepath.endsWith(".fzn")) {
            run(filepath);
            return;
        }
    }

    // Get model interface to obtain parameters
    QStringList args;
    args << "--model-interface-only";
    for (QString dzn: currentAdditionalDataFiles)
        args << dzn;
    args << filepath;

    auto p = new MznProcess(this);
    connect(p, qOverload<int, QProcess::ExitStatus>(&MznProcess::finished), [=](int code, QProcess::ExitStatus status) {
        p->deleteLater();

        QString additionalCmdlineParams;
        QStringList additionalDataFiles;

        if (code == 0 && status == QProcess::NormalExit) {
            auto jdoc = QJsonDocument::fromJson(p->readAllStandardOutput());
            if (jdoc.isObject() && jdoc.object()["input"].isObject() && jdoc.object()["method"].isString()) {
                QJsonObject inputArgs = jdoc.object()["input"].toObject();
                QStringList undefinedArgs = inputArgs.keys();
                if (undefinedArgs.size() > 0) {
                    QStringList params;
                    paramDialog->getParams(undefinedArgs, project.dataFiles(), params, additionalDataFiles);
                    if (additionalDataFiles.isEmpty()) {
                        if (params.size()==0) {
                            procFinished(0,false);
                            return;
                        }
                        for (int i=0; i<undefinedArgs.size(); i++) {
                            if (params[i].isEmpty()) {
                                if (! (inputArgs[undefinedArgs[i]].isObject() && inputArgs[undefinedArgs[i]].toObject().contains("optional")) ) {
                                    QMessageBox::critical(this, "Undefined parameter", "The parameter '" + undefinedArgs[i] + "' is undefined.");
                                    procFinished(0);
                                    return;
                                }
                            } else {
                                additionalCmdlineParams += undefinedArgs[i]+"="+params[i]+"; ";
                            }
                        }
                    }
                }
            } else {
                addOutput("<p style='color:red'>Error when checking model parameters:</p>");
                addOutput(p->readAllStandardError(), false);
                QMessageBox::critical(this, "Internal error", "Could not determine model parameters");
                procFinished(0);
                return;
            }
        }
        for (QString dzn: currentAdditionalDataFiles) {
            additionalDataFiles.append(dzn);
        }

        switch (compileMode) {
        case CM_RUN:
            run(filepath, additionalDataFiles);
            break;
        case CM_COMPILE:
            compile(filepath, additionalDataFiles);
            break;
        case CM_PROFILE:
            compile(filepath, additionalDataFiles, true);
            break;
        }
    });
    connect(p, &QProcess::errorOccurred, [=](QProcess::ProcessError e) {
        p->deleteLater();
        auto message = p->readAllStandardError();
        if (!message.isEmpty()) {
            addOutput(message,false);
        }
        procFinished(0);
        if (e == QProcess::FailedToStart) {
            QMessageBox::critical(this, "MiniZinc IDE", "Failed to start MiniZinc. Check your path settings.");
        } else {
            QMessageBox::critical(this, "MiniZinc IDE", "Unknown error while executing MiniZinc: error code "+ QString().number(e));
        }
    });

    p->run(*sc, args, QFileInfo(filepath).absolutePath());
}

QString MainWindow::setElapsedTime(qint64 elapsed_t)
{
    int hours =  elapsed_t / 3600000;
    int minutes = (elapsed_t % 3600000) / 60000;
    int seconds = (elapsed_t % 60000) / 1000;
    int msec = (elapsed_t % 1000);
    QString elapsed;
    if (hours > 0)
        elapsed += QString().number(hours)+"h ";
    if (hours > 0 || minutes > 0)
        elapsed += QString().number(minutes)+"m ";
    if (hours > 0 || minutes > 0 || seconds > 0)
        elapsed += QString().number(seconds)+"s";
    if (hours==0 && minutes==0)
        elapsed += " "+QString().number(msec)+"msec";
    QString timeLimit;
    auto sc = getCurrentSolverConfig();
    if (sc && sc->timeLimit > 0) {
        timeLimit += " / ";
        int tl_hours = sc->timeLimit / 3600;
        int tl_minutes = (sc->timeLimit % 3600) / 60;
        int tl_seconds = sc->timeLimit % 60;
        if (tl_hours > 0)
            timeLimit += QString().number(tl_hours)+"h ";
        if (tl_hours > 0 || tl_minutes > 0)
            timeLimit += QString().number(tl_minutes)+"m ";
        timeLimit += QString().number(tl_seconds)+"s";
    }
    statusLabel->setText(elapsed+timeLimit);
    return elapsed;
}

void MainWindow::statusTimerEvent()
{
    QString txt = "Running.";
    qint64 time = 0;
    if (compileProcess) {
        time = compileProcess->timeElapsed();
    } else if (solveProcess) {
        time = solveProcess->timeElapsed();
    }
    int dots = time / 5000000000;
    for (int i = 0; i < dots; i++) {
        txt += ".";
    }
    ui->statusbar->showMessage(txt);
    setElapsedTime(time);
}

//void MainWindow::openJSONViewer(void)
//{
//    if (curHtmlWindow==-1) {
//        QVector<VisWindowSpec> specs;
//        for (int i=0; i<JSONOutput.size(); i++) {
//            QString url = JSONOutput[i].first();
//            Qt::DockWidgetArea area = Qt::TopDockWidgetArea;
//            if (JSONOutput[i][1]=="top") {
//                area = Qt::TopDockWidgetArea;
//            } else if (JSONOutput[i][1]=="bottom") {
//                area = Qt::BottomDockWidgetArea;
//            }
//            url.remove(QRegExp("[\\n\\t\\r]"));
//            specs.append(VisWindowSpec(url,area));
//        }
//        QFileInfo htmlWindowTitleFile(curFilePath);
//        HTMLWindow* htmlWindow = new HTMLWindow(specs, this, htmlWindowTitleFile.fileName());
//        curHtmlWindow = htmlWindow->getId();
//        htmlWindow->init();
//        connect(htmlWindow, SIGNAL(closeWindow(int)), this, SLOT(closeHTMLWindow(int)));
//        htmlWindow->show();
//    }
//    for (int i=0; i<JSONOutput.size(); i++) {
//        JSONOutput[i].pop_front();
//        JSONOutput[i].pop_front();
//        if (htmlWindows[curHtmlWindow]) {
//            if (isJSONinitHandler) {
//                htmlWindows[curHtmlWindow]->initJSON(i, JSONOutput[i].join(' '));
//            } else {
//                htmlWindows[curHtmlWindow]->addSolution(i, JSONOutput[i].join(' '));
//            }
//        }
//    }
//}

//void MainWindow::finishJSONViewer(void)
//{
//    if (curHtmlWindow >= 0 && htmlWindows[curHtmlWindow]) {
//        htmlWindows[curHtmlWindow]->finish(elapsedTime.elapsed());
//    }
//}

void MainWindow::compile(const QString& model, const QStringList& data, bool profile)
{
    auto sc = getCurrentSolverConfig();
    if (!requireMiniZinc() || !sc) {
        return;
    }

    QFileInfo fi(model);

    QTemporaryDir* fznTmpDir = new QTemporaryDir;
    if (!fznTmpDir->isValid()) {
        QMessageBox::critical(this, "MiniZinc IDE", "Could not create temporary directory for compilation.");
        return;
    }
    cleanupTmpDirs.append(fznTmpDir);
    QString fzn = fznTmpDir->path() + "/" + fi.baseName() + ".fzn";

    if (compileProcess) {
        compileProcess->disconnect();
        compileProcess->terminate();
        compileProcess->deleteLater();
    }

    compileProcess = new MznProcess(this);
    QStringList args;
    args << "-c"
         << "-o" << fzn
         << model
         << data;
    if (profile) {
        args << "--output-paths-to-stdout"
             << "--output-detailed-timing";
    }
    connect(compileProcess, &MznProcess::readyReadStandardError, [=]() {
       compileProcess->setReadChannel(QProcess::StandardError);
       while (compileProcess->canReadLine()) {
           auto line = QString::fromUtf8(compileProcess->readLine());
           outputStdErr(line);
       }
    });
    connect(compileProcess, qOverload<int, QProcess::ExitStatus>(&MznProcess::finished), [=](int exitCode, QProcess::ExitStatus exitStatus) {
        timer->stop();
        auto output = QString::fromUtf8(compileProcess->readAllStandardOutput());
        compileProcess->deleteLater();
        compileProcess = nullptr;

        if (exitStatus == QProcess::CrashExit) {
            QMessageBox::critical(this, "MiniZinc IDE", "MiniZinc crashed unexpectedly.");
            return;
        }

        if (exitCode == 0) {
            if (profile) {
                profileCompiledFzn(output);
            } else {
                openCompiledFzn(fzn);
            }
        }

        procFinished(exitCode);
    });
    connect(compileProcess, &MznProcess::errorOccurred, [=](QProcess::ProcessError e) {
        timer->stop();
        compileProcess->deleteLater();
        compileProcess = nullptr;
        if (e == QProcess::FailedToStart) {
            QMessageBox::critical(this, "MiniZinc IDE", "Failed to start MiniZinc. Check your path settings.");
        } else {
            QMessageBox::critical(this, "MiniZinc IDE", "Unknown error while executing MiniZinc: error code " + QString().number(e));
        }
    });
    compileProcess->run(*sc, args, fi.absolutePath());
    timer->start(1000);
}

void MainWindow::run(const QString& model, const QStringList& data, const QStringList& extraArgs)
{
    auto sc = getCurrentSolverConfig();
    if (!requireMiniZinc() || !sc) {
        return;
    }

    if (solveProcess) {
        solveProcess->terminate();
        solveProcess->deleteLater();
    }

    solveProcess = new SolveProcess(this);
    connect(solveProcess, &SolveProcess::solutionOutput, [=](QString solution) {
        addOutput(solution, false);
    });
    connect(solveProcess, &SolveProcess::optimal, [=](QString line) {
        addOutput(line, false);
    });
    connect(solveProcess, &SolveProcess::htmlOutput, [=](QString html) {
        addOutput(html, true);
    });
    connect(solveProcess, &SolveProcess::jsonInit, [=](const QString& path, Qt::DockWidgetArea area, const QString& data) {

    });
    connect(solveProcess, &SolveProcess::jsonOutput, [=](const QString& path, Qt::DockWidgetArea area, const QString& data) {

    });
    connect(solveProcess, &SolveProcess::stdErrorOutput, this, &MainWindow::outputStdErr);
    connect(solveProcess, qOverload<int, QProcess::ExitStatus>(&MznProcess::finished), [=](int exitCode, QProcess::ExitStatus exitStatus) {
        timer->stop();
        solveProcess->deleteLater();
        solveProcess = nullptr;
        if (exitStatus == QProcess::CrashExit) {
            QMessageBox::critical(this, "MiniZinc IDE", "MiniZinc crashed unexpectedly.");
            procFinished(0);
            return;
        }
        procFinished(exitCode);
    });
    connect(solveProcess, &MznProcess::errorOccurred, [=](QProcess::ProcessError e) {
        timer->stop();
        solveProcess->deleteLater();
        solveProcess = nullptr;
        if (e == QProcess::FailedToStart) {
            QMessageBox::critical(this, "MiniZinc IDE", "Failed to start MiniZinc. Check your path settings.");
        } else {
            QMessageBox::critical(this, "MiniZinc IDE", "Unknown error while executing MiniZinc: error code " + QString().number(e));
        }
    });
    solveProcess->solve(*sc, model, data, extraArgs);
    timer->start(1000);
}

bool MainWindow::runForSubmission(const QString &modelFile, const QString &dataFile, int timeout, QTextStream &outstream)
{
    if (!QFileInfo(modelFile).exists() || !QFileInfo(dataFile).exists())
        return false;
    outputBuffer = &outstream;
//    compileMode = CM_RUN;
    runTimeout = timeout;
    curHtmlWindow = -1;
    updateUiProcessRunning(true);
    on_actionSplit_triggered();
    currentAdditionalDataFiles.clear();
    currentAdditionalDataFiles.push_back(dataFile);

    QString checkFile = modelFile;
    checkFile.replace(checkFile.length()-1,1,"c");
    bool haveChecker = project.containsFile(checkFile) || project.containsFile(checkFile+".mzn");

    QStringList outputModeArgs;
    if (haveChecker /*&& ui->conf_check_solutions->isChecked()*/ && MznDriver::get().version() >= QVersionNumber(2,4,4)) {
        outputModeArgs.push_back("--output-mode");
        outputModeArgs.push_back("checker");
    }
//    checkArgs(modelFile, false, outputModeArgs);
    return true;
}

void MainWindow::resolve(int htmlWindowIdentifier, const QString &data)
{
    QTemporaryDir* dataTmpDir = new QTemporaryDir;
    if (!dataTmpDir->isValid()) {
        QMessageBox::critical(this, "MiniZinc IDE", "Could not create temporary directory for compilation.");
        return;
    } else {
        cleanupTmpDirs.append(dataTmpDir);
        QString filepath = dataTmpDir->path()+"/untitled_data.json";
        QFile dataFile(filepath);
        if (dataFile.open(QIODevice::ReadWrite)) {
            QTextStream ts(&dataFile);
            ts << data;
            dataFile.close();
            QStringList dataFiles;
            dataFiles.push_back(filepath);
            curHtmlWindow = htmlWindowIdentifier;
            run(htmlWindowModels[htmlWindowIdentifier], dataFiles);
        } else {
            QMessageBox::critical(this, "MiniZinc IDE", "Could not write temporary model file.");
        }
    }
}

QString MainWindow::currentSolverConfigName(void) {
    SolverConfiguration* sc = getCurrentSolverConfig();
    return sc ? sc->name() : "None";
}

int MainWindow::addHtmlWindow(HTMLWindow *w)
{
    htmlWindows.push_back(w);
    htmlWindowModels.push_back(curFilePath);
    return htmlWindows.size()-1;
}

void MainWindow::closeHTMLWindow(int identifier)
{
    on_actionStop_triggered();
    if (identifier==curHtmlWindow) {
        curHtmlWindow = -1;
    }
    htmlWindows[identifier] = nullptr;
}

void MainWindow::selectJSONSolution(HTMLPage* source, int n)
{
    if (curHtmlWindow >= 0 && htmlWindows[curHtmlWindow]) {
        htmlWindows[curHtmlWindow]->selectSolution(source,n);
    }
}

void MainWindow::procFinished(int exitCode, bool showTime) {
    if (exitCode != 0) {
        addOutput("<div style='color:red;'>Process finished with non-zero exit code "+QString().number(exitCode)+"</div>");
    }
    updateUiProcessRunning(false);
    qint64 time = 0;
    if (solveProcess) {
        time = solveProcess->timeElapsed();
    } else if (compileProcess) {
        time = compileProcess->timeElapsed();
    }
    QString elapsedTime = setElapsedTime(time);
    ui->statusbar->clearMessage();
    if (showTime) {
        addOutput("<div class='mznnotice'>Finished in "+elapsedTime+"</div>");
    }
    delete tmpDir;
    tmpDir = nullptr;
    outputBuffer = nullptr;
    emit(finished());
}

void MainWindow::saveFile(CodeEditor* ce, const QString& f)
{
    QString filepath = f;
    int tabIndex = ui->tabWidget->indexOf(ce);
    if (filepath=="") {
        if (ce != curEditor) {
            ui->tabWidget->setCurrentIndex(tabIndex);
        }
        QString dialogPath = ce->filepath.isEmpty() ? getLastPath()+"/"+ce->filename: ce->filepath;
        QString selectedFilter = "Other (*)";
        if (dialogPath.endsWith(".mzn") || (ce->filepath.isEmpty() && ce->filename=="Playground"))
            selectedFilter = "MiniZinc model (*.mzn)";
        else if (dialogPath.endsWith(".dzn") || dialogPath.endsWith(".json"))
            selectedFilter = "MiniZinc data (*.dzn *.json)";
        else if (dialogPath.endsWith(".fzn"))
            selectedFilter = "FlatZinc (*.fzn)";
        else if (dialogPath.endsWith(".mzc"))
            selectedFilter = "MiniZinc solution checker (*.mzc)";
        filepath = QFileDialog::getSaveFileName(this,"Save file",dialogPath,"MiniZinc model (*.mzn);;MiniZinc data (*.dzn *.json);;MiniZinc solution checker (*.mzc);;FlatZinc (*.fzn);;Other (*)",&selectedFilter);
        if (!filepath.isNull()) {
            setLastPath(QFileInfo(filepath).absolutePath()+fileDialogSuffix);
        }
    }
    if (!filepath.isEmpty()) {
        if (filepath != ce->filepath && IDE::instance()->hasFile(filepath)) {
            QMessageBox::warning(this,"MiniZinc IDE","Cannot overwrite open file.",
                                 QMessageBox::Ok);

        } else {
            IDE::instance()->fsWatch.removePath(filepath);
            QFile file(filepath);
            if (file.open(QFile::WriteOnly | QFile::Text)) {
                QTextStream out(&file);
                out.setCodec(QTextCodec::codecForName("UTF-8"));
                out << ce->document()->toPlainText();
                file.close();
                if (filepath != ce->filepath) {
                    QTextDocument* newdoc =
                            IDE::instance()->addDocument(filepath,ce->document(),ce);
                    ce->setDocument(newdoc);
                    if (ce->filepath != "") {
                        IDE::instance()->removeEditor(ce->filepath,ce);
                    }
                    project.removeFile(ce->filepath);
                    project.addFile(ui->projectView, projectSort, filepath);
                    ce->filepath = filepath;
                }
                ce->document()->setModified(false);
                ce->filename = QFileInfo(filepath).fileName();
                ui->tabWidget->setTabText(tabIndex,ce->filename);
                updateRecentFiles(filepath);
                if (ce==curEditor)
                    tabChange(tabIndex);
            } else {
                QMessageBox::warning(this,"MiniZinc IDE","Could not save file");
            }
            IDE::instance()->fsWatch.addPath(filepath);
        }
    }
}

void MainWindow::fileRenamed(const QString& oldPath, const QString& newPath)
{
    for (int i=0; i<ui->tabWidget->count(); i++) {
        CodeEditor* ce = static_cast<CodeEditor*>(ui->tabWidget->widget(i));
        if (ce->filepath==oldPath) {
            ce->filepath = newPath;
            ce->filename = QFileInfo(newPath).fileName();
            IDE::instance()->renameFile(oldPath,newPath);
            ui->tabWidget->setTabText(i,ce->filename);
            updateRecentFiles(newPath);
            if (ce==curEditor)
                tabChange(i);
        }
    }
}

void MainWindow::on_actionSave_triggered()
{
    if (curEditor) {
        saveFile(curEditor,curEditor->filepath);
    }
}

void MainWindow::on_actionSave_as_triggered()
{
    if (curEditor) {
        saveFile(curEditor,QString());
    }
}

void MainWindow::on_actionQuit_triggered()
{
    qApp->closeAllWindows();
    if (IDE::instance()->mainWindows.size()==0) {
        IDE::instance()->quit();
    }
}

void MainWindow::on_actionStop_triggered()
{
    ui->actionStop->setEnabled(false);

    if (solveProcess) {
        solveProcess->disconnect();
        solveProcess->terminate();
        solveProcess->deleteLater();
        solveProcess = nullptr;
        addOutput("<div class='mznnotice'>Stopped.</div>");
        procFinished(0);
    }

    if (compileProcess) {
        compileProcess->disconnect();
        compileProcess->terminate();
        compileProcess->deleteLater();
        compileProcess = nullptr;
        addOutput("<div class='mznnotice'>Stopped.</div>");
        procFinished(0);
    }
}

void MainWindow::openCompiledFzn(const QString& fzn)
{

    QFile file(fzn);
    int fsize = file.size() / (1024 * 1024);
    if (fsize > 10) {
        QMessageBox::StandardButton sb =
                QMessageBox::warning(this, "MiniZinc IDE",
                                     QString("Compilation resulted in a large FlatZinc file (")+
                                     QString().setNum(fsize)+" MB). Opening "
                                     "the file may slow down the IDE and potentially "
                                     "affect its stability. Do you want to open it anyway, save it, or discard the file?",
                                     QMessageBox::Open | QMessageBox::Discard | QMessageBox::Save);
        switch (sb) {
        case QMessageBox::Save:
        {
            bool success = true;
            do {
                QString savepath = QFileDialog::getSaveFileName(this,"Save FlatZinc",getLastPath(),"FlatZinc files (*.fzn)");
                if (!savepath.isNull() && !savepath.isEmpty()) {
                    QFile oldfile(savepath);
                    if (oldfile.exists()) {
                        if (!oldfile.remove()) {
                            success = false;
                        }
                    }
                    file.copy(savepath);
                }
            } while (!success);
            return;
        }
        case QMessageBox::Discard:
            return;
        default:
            break;
        }
    }
    openFile(fzn, !fzn.endsWith(".mzc"));
}

void MainWindow::profileCompiledFzn(const QString& output)
{
    auto lines = output.split(QRegExp("[\r\n]"), QString::SkipEmptyParts);
    QRegularExpression path_re("^(.+)\\t(.+)\\t(.*;)$");
    QRegularExpression count_re("([^|;]*)\\|(\\d+)\\|(\\d+)\\|(\\d+)\\|(\\d+)\\|([^;]*);");
    QRegularExpression time_re("^%%%mzn-stat: profiling=\\[\"(.*)\",(\\d*),(\\d*)\\]$");

    typedef QMap<QString,QVector<BracketData*>> CoverageMap;
    CoverageMap ce_coverage;

    for (int i=0; i<ui->tabWidget->count(); i++) {
        CodeEditor* ce = static_cast<CodeEditor*>(ui->tabWidget->widget(i));
        if (ce->filepath.endsWith(".mzn")) {
            QTextBlock tb = ce->document()->begin();
            QVector<BracketData*> coverage;
            while (tb.isValid()) {
                BracketData* bd = static_cast<BracketData*>(tb.userData());
                if (bd==nullptr) {
                    bd = new BracketData;
                    tb.setUserData(bd);
                }
                bd->d.reset();
                coverage.push_back(bd);
                tb = tb.next();
            }
            ce_coverage.insert(ce->filepath,coverage);
        }
    }

    int totalCons=1;
    int totalVars=1;
    int totalTime=1;
    for (auto line : lines) {
        QRegularExpressionMatch path_match = path_re.match(line);
        if (path_match.hasMatch()) {
            QString paths = path_match.captured(3);
            QRegularExpressionMatchIterator match = count_re.globalMatch(paths);
            if (match.hasNext()) {
                int min_line_covered = -1;
                CoverageMap::iterator fileMatch;
                while (match.hasNext()) {
                    QRegularExpressionMatch m = match.next();
                    auto tryMatch = ce_coverage.find(m.captured(1));
                    if (tryMatch != ce_coverage.end()) {
                        fileMatch = tryMatch;
                        min_line_covered = m.captured(2).toInt();
                    }
                }
                if (min_line_covered > 0 && min_line_covered <= fileMatch.value().size()) {
                    if (path_match.captured(1)[0].isDigit()) {
                        fileMatch.value()[min_line_covered-1]->d.con++;
                        totalCons++;
                    } else {
                        fileMatch.value()[min_line_covered-1]->d.var++;
                        totalVars++;
                    }
                }
            }
        } else {
            QRegularExpressionMatch time_match = time_re.match(line);
            if (time_match.hasMatch()) {
                CoverageMap::iterator fileMatch = ce_coverage.find(time_match.captured(1));
                if (fileMatch != ce_coverage.end()) {
                    int line_no = time_match.captured(2).toInt();
                    if (line_no > 0 && line_no <= fileMatch.value().size()) {
                        fileMatch.value()[line_no-1]->d.ms = time_match.captured(3).toInt();
                        totalTime += fileMatch.value()[line_no-1]->d.ms;
                    }
                }
            }
        }
    }
    for (auto& coverage: ce_coverage) {
        for(auto data: coverage){
            data->d.totalCon = totalCons;
            data->d.totalVar = totalVars;
            data->d.totalMs = totalTime;
        }
    }
    curEditor->repaint();
}

void MainWindow::on_actionCompile_triggered()
{
    compileOrRun(CM_COMPILE);
}

void MainWindow::on_actionClear_output_triggered()
{
    progressBar->setHidden(true);
    ui->outputConsole->document()->clear();
}

void MainWindow::setEditorFont(QFont font)
{
    QTextCharFormat format;
    format.setFont(font);

    ui->outputConsole->setFont(font);
    QTextCursor cursor(ui->outputConsole->document());
    cursor.movePosition(QTextCursor::Start);
    cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
    cursor.mergeCharFormat(format);
    for (int i=0; i<ui->tabWidget->count(); i++) {
        CodeEditor* ce = static_cast<CodeEditor*>(ui->tabWidget->widget(i));
        ce->setEditorFont(font);
    }
}

void MainWindow::on_actionBigger_font_triggered()
{
    editorFont.setPointSize(editorFont.pointSize()+1);
    IDE::instance()->setEditorFont(editorFont);
}

void MainWindow::on_actionSmaller_font_triggered()
{
    editorFont.setPointSize(std::max(5, editorFont.pointSize()-1));
    IDE::instance()->setEditorFont(editorFont);
}

void MainWindow::on_actionDefault_font_size_triggered()
{
    editorFont.setPointSize(13);
    IDE::instance()->setEditorFont(editorFont);
}

#ifndef MINIZINC_IDE_BUILD
#define MINIZINC_IDE_BUILD ""
#endif

void MainWindow::on_actionAbout_MiniZinc_IDE_triggered()
{
    QString buildString(MINIZINC_IDE_BUILD);
    if (!buildString.isEmpty())
        buildString += "\n";
    QMessageBox::about(this, "The MiniZinc IDE", QString("The MiniZinc IDE\n\nVersion ")+IDE::instance()->applicationVersion()+"\n"+
                       buildString+"\n"
                       "Copyright Monash University, NICTA, Data61 2013-" + BUILD_YEAR + "\n\n"+
                       "This program is provided under the terms of the Mozilla Public License Version 2.0. It uses the Qt toolkit, available from qt-project.org.");
}

QVector<CodeEditor*> MainWindow::collectCodeEditors(QVector<QStringList>& locs) {
  QVector<CodeEditor*> ces;
  ces.resize(locs.size());
  // Open each file in the path
  for (int p = 0; p < locs.size(); p++) {
    QStringList& elements = locs[p];

    QString filename = elements[0];
    QUrl url = QUrl::fromLocalFile(filename);
    QFileInfo urlinfo(url.toLocalFile());

    bool notOpen = true;
    if (filename != "") {
      for (int i=0; i<ui->tabWidget->count(); i++) {
          CodeEditor* ce = static_cast<CodeEditor*>(ui->tabWidget->widget(i));
          QFileInfo ceinfo(ce->filepath);

          if (ceinfo.canonicalFilePath() == urlinfo.canonicalFilePath()) {
              ces[p] = ce;
              if(p == locs.size()-1) {
                  ui->tabWidget->setCurrentIndex(i);
              }
              notOpen = false;
              break;
          }
      }
      if (notOpen && filename.size() > 0) {
        openFile(url.toLocalFile(), false, false);
        CodeEditor* ce = static_cast<CodeEditor*>(ui->tabWidget->widget(ui->tabWidget->count()-1));
        QFileInfo ceinfo(ce->filepath);

        if (ceinfo.canonicalFilePath() == urlinfo.canonicalFilePath()) {
          ces[p] = ce;
        } else {
          throw InternalError("Code editor file path does not match URL file path");
        }
      }
    } else {
       CodeEditor* ce = static_cast<CodeEditor*>(ui->tabWidget->widget(ui->tabWidget->currentIndex()));
       ces[p] = ce;
    }
  }
  return ces;
}

void MainWindow::find(bool fwd, bool forceNoWrapAround)
{
    const QString& toFind = ui->find->text();
    QTextDocument::FindFlags flags;
    if (!fwd)
        flags |= QTextDocument::FindBackward;
    bool ignoreCase = ui->check_case->isChecked();
    if (!ignoreCase)
        flags |= QTextDocument::FindCaseSensitively;
    bool wrap = !forceNoWrapAround && ui->check_wrap->isChecked();

    QTextCursor cursor(curEditor->textCursor());
    int hasWrapped = wrap ? 0 : 1;
    while (hasWrapped < 2) {
        if (ui->check_re->isChecked()) {
            QRegExp re(toFind, ignoreCase ? Qt::CaseInsensitive : Qt::CaseSensitive);
            if (!re.isValid()) {
                ui->not_found->setText("invalid");
                return;
            }
            cursor = curEditor->document()->find(re,cursor,flags);
        } else {
            cursor = curEditor->document()->find(toFind,cursor,flags);
        }
        if (cursor.isNull()) {
            hasWrapped++;
            cursor = curEditor->textCursor();
            if (fwd) {
                cursor.setPosition(0);
            } else {
                cursor.movePosition(QTextCursor::End);
            }
        } else {
            ui->not_found->setText("");
            curEditor->setTextCursor(cursor);
            break;
        }
    }
    if (hasWrapped==2) {
        ui->not_found->setText("not found");
    }

}

#define major_sep ';'
#define minor_sep '|'
QVector<QStringList> getBlocksFromPath(QString& path) {
  QVector<QStringList> locs;
  QStringList blocks = path.split(major_sep);
  foreach(QString block, blocks) {
    QStringList elements = block.split(minor_sep);
    if(elements.size() >= 5) {
      bool ok = false;
      if(elements.size() > 5) elements[5].toInt(&ok);
      elements.erase(elements.begin()+(ok ? 6 : 5), elements.end());
      locs.append(elements);
    }
  }
  return locs;
}

void MainWindow::highlightPath(QString& path, int index) {
  // Build list of blocks to be highlighted
  QVector<QStringList> locs = getBlocksFromPath(path);
  if(locs.size() == 0) return;
  QVector<CodeEditor*> ces = collectCodeEditors(locs);
  if(ces.size() != locs.size()) return;

  int b = Qt::red;
  int t = Qt::yellow;
  QColor colour = static_cast<Qt::GlobalColor>((index % (t-b)) + b);

  int strans = 25;
  int trans = strans;
  int tstep = (250-strans) / locs.size();

  for(int p = 0; p < locs.size(); p++) {
    QStringList& elements = locs[p];
    CodeEditor* ce = ces[p];

    bool ok;
    int sl = elements[1].toInt(&ok);
    int sc = elements[2].toInt(&ok);
    int el = elements[3].toInt(&ok);
    int ec = elements[4].toInt(&ok);
    if(elements.size() == 6)
        trans = elements[5].toInt(&ok);
    if (ok) {
      colour.setAlpha(trans);
      trans = trans < 250 ? trans+tstep : strans;

      Highlighter& hl = ce->getHighlighter();
      hl.addFixedBg(sl,sc,el,ec,colour,path);
      hl.rehighlight();

      ce->setTextCursor(QTextCursor(ce->document()->findBlockByLineNumber(el)));
    }
  }
}

void MainWindow::errorClicked(const QUrl & anUrl)
{
    QUrl url = anUrl;

    if(url.scheme() == "highlight") {
      // Reset the highlighters
      for (int i=0; i<ui->tabWidget->count(); i++) {
          CodeEditor* ce = static_cast<CodeEditor*>(ui->tabWidget->widget(i));
          Highlighter& hl = ce->getHighlighter();
          hl.clearFixedBg();
          hl.rehighlight();
      }

      QString query = url.query();
      QStringList conflictSet = query.split("&");

      for(int c = 0; c<conflictSet.size(); c++) {
        QString& Q = conflictSet[c];
        highlightPath(Q, c);
      }
      return;
    }

    QString query = url.query();
    url.setQuery("");
    url.setScheme("file");
    QFileInfo urlinfo(url.toLocalFile());
    IDE::instance()->stats.errorsClicked++;
    for (int i=0; i<ui->tabWidget->count(); i++) {
        CodeEditor* ce = static_cast<CodeEditor*>(ui->tabWidget->widget(i));
        QFileInfo ceinfo(ce->filepath);
        if (ceinfo.canonicalFilePath() == urlinfo.canonicalFilePath()) {
            QRegExp re_line("line=([0-9]+)");
            if (re_line.indexIn(query) != -1) {
                bool ok;
                int line = re_line.cap(1).toInt(&ok);
                if (ok) {
                    int col = 1;
                    QRegExp re_col("column=([0-9]+)");
                    if (re_col.indexIn(query) != -1) {
                        bool ok;
                        col = re_col.cap(1).toInt(&ok);
                        if (!ok)
                            col = 1;
                    }
                    QTextBlock block = ce->document()->findBlockByNumber(line-1);
                    if (block.isValid()) {
                        QTextCursor cursor = ce->textCursor();
                        cursor.setPosition(block.position()+col-1);
                        ce->setFocus();
                        ce->setTextCursor(cursor);
                        ce->centerCursor();
                        ui->tabWidget->setCurrentIndex(i);
                    }
                }
            }
        }
    }
}

void MainWindow::on_actionManage_solvers_triggered(bool addNew)
{
    QSettings settings;
    settings.beginGroup("ide");
    bool checkUpdates = settings.value("checkforupdates21",false).toBool();
    settings.endGroup();

    SolverDialog sd(addNew);
    sd.exec();

    ui->config_window->loadConfigs();

    settings.beginGroup("ide");
    if (!checkUpdates && settings.value("checkforupdates21",false).toBool()) {
        settings.setValue("lastCheck21",QDate::currentDate().addDays(-2).toString());
        IDE::instance()->checkUpdate();
    }
    settings.endGroup();

    settings.beginGroup("minizinc");
    settings.setValue("mznpath", MznDriver::get().mznDistribPath());
    settings.endGroup();
}

void MainWindow::on_actionFind_triggered()
{
    incrementalFindCursor = curEditor->textCursor();
    incrementalFindCursor.setPosition(std::min(curEditor->textCursor().anchor(), curEditor->textCursor().position()));
    if (curEditor->textCursor().hasSelection()) {
        ui->find->setText(curEditor->textCursor().selectedText());
    }
    ui->not_found->setText("");
    ui->find->selectAll();
    ui->findFrame->raise();
    ui->findFrame->show();
    ui->find->setFocus();
}

void MainWindow::on_actionReplace_triggered()
{
    incrementalFindCursor = curEditor->textCursor();
    incrementalFindCursor.setPosition(std::min(curEditor->textCursor().anchor(), curEditor->textCursor().position()));
    if (curEditor->textCursor().hasSelection()) {
        ui->find->setText(curEditor->textCursor().selectedText());
    }
    ui->not_found->setText("");
    ui->find->selectAll();
    ui->findFrame->raise();
    ui->findFrame->show();
}

void MainWindow::on_actionSelect_font_triggered()
{
    bool ok;
    QFont newFont = QFontDialog::getFont(&ok,editorFont,this);
    if (ok) {
        editorFont = newFont;
        IDE::instance()->setEditorFont(editorFont);
    }
}

void MainWindow::on_actionGo_to_line_triggered()
{
    if (curEditor==nullptr)
        return;
    GoToLineDialog gtl;
    if (gtl.exec()==QDialog::Accepted) {
        bool ok;
        int line = gtl.getLine(&ok);
        if (ok) {
            QTextBlock block = curEditor->document()->findBlockByNumber(line-1);
            if (block.isValid()) {
                QTextCursor cursor = curEditor->textCursor();
                cursor.setPosition(block.position());
                curEditor->setTextCursor(cursor);
            }
        }
    }
}

void MainWindow::checkMznPath(const QString& mznPath)
{
    auto& driver = MznDriver::get();
    try {
        driver.setLocation(mznPath);
    } catch (Exception& e) {
        int ret = QMessageBox::warning(this, "MiniZinc IDE",
                                       e.message() + "\nDo you want to open the settings dialog?",
                                       QMessageBox::Ok | QMessageBox::Cancel);
        if (ret == QMessageBox::Ok)
            on_actionManage_solvers_triggered();
    }

    bool haveMzn = (driver.isValid() && driver.solvers().size() > 0);
    ui->actionRun->setEnabled(haveMzn);
    ui->actionProfile_compilation->setEnabled(haveMzn);
    ui->actionCompile->setEnabled(haveMzn);
    ui->actionEditSolverConfig->setEnabled(haveMzn);
    ui->actionSubmit_to_MOOC->setEnabled(haveMzn);
    if (!haveMzn)
        ui->configWindow_dockWidget->hide();
    ui->config_window->init();
}

void MainWindow::on_actionShift_left_triggered()
{
    QTextCursor cursor = curEditor->textCursor();
    QTextBlock block = curEditor->document()->findBlock(cursor.selectionStart());
    QTextBlock endblock = curEditor->document()->findBlock(cursor.selectionEnd());
    bool atBlockStart = cursor.selectionEnd() == endblock.position();
    if (block==endblock || !atBlockStart)
        endblock = endblock.next();
    QRegExp white("\\s");
    QRegExp twowhite("\\s\\s");
    cursor.beginEditBlock();
    do {
        cursor.setPosition(block.position());
        if (block.length() > 2) {
            cursor.movePosition(QTextCursor::Right,QTextCursor::KeepAnchor,2);
            if (twowhite.indexIn(cursor.selectedText()) != 0) {
                cursor.movePosition(QTextCursor::Left,QTextCursor::KeepAnchor,1);
            }
            if (white.indexIn(cursor.selectedText()) == 0) {
                cursor.removeSelectedText();
            }
        }
        block = block.next();
    } while (block.isValid() && block != endblock);
    cursor.endEditBlock();
}

void MainWindow::on_actionShift_right_triggered()
{
    QTextCursor cursor = curEditor->textCursor();
    QTextBlock block = curEditor->document()->findBlock(cursor.selectionStart());
    QTextBlock endblock = curEditor->document()->findBlock(cursor.selectionEnd());
    bool atBlockStart = cursor.selectionEnd() == endblock.position();
    if (block==endblock || !atBlockStart)
        endblock = endblock.next();
    cursor.beginEditBlock();
    do {
        cursor.setPosition(block.position());
        cursor.insertText("  ");
        block = block.next();
    } while (block.isValid() && block != endblock);
    cursor.endEditBlock();
}

void MainWindow::on_actionHelp_triggered()
{
    IDE::instance()->help();
}

void MainWindow::on_actionNew_project_triggered()
{
    MainWindow* mw = new MainWindow;
    QPoint p = pos();
    mw->move(p.x()+20, p.y()+20);
    mw->show();
}

bool MainWindow::isEmptyProject(void)
{
    if (ui->tabWidget->count() == 0) {
        return project.isUndefined();
    }
    if (ui->tabWidget->count() == 1) {
        CodeEditor* ce = static_cast<CodeEditor*>(ui->tabWidget->widget(0));
        return ce->filepath == "" && !ce->document()->isModified() && !project.isModified();
    }
    return false;
}

void MainWindow::openProject(const QString& fileName)
{
    if (!fileName.isEmpty()) {
        IDE::PMap& pmap = IDE::instance()->projects;
        IDE::PMap::iterator it = pmap.find(fileName);
        if (it==pmap.end()) {
            if (isEmptyProject()) {
                int closeTab = ui->tabWidget->count()==1 ? 0 : -1;
                loadProject(fileName);
                if (closeTab > 0 && ui->tabWidget->count()>1) {
                    CodeEditor* ce = static_cast<CodeEditor*>(ui->tabWidget->widget(closeTab));
                    if (ce->filepath == "")
                        tabCloseRequest(closeTab);
                }
            } else {
                MainWindow* mw = new MainWindow(fileName);
                QPoint p = pos();
                mw->move(p.x()+20, p.y()+20);
                mw->show();
            }
        } else {
            it.value()->raise();
            it.value()->activateWindow();
        }
    }
}

void MainWindow::updateRecentProjects(const QString& p) {
    if (!p.isEmpty()) {
        IDE::instance()->addRecentProject(p);
    }
    ui->menuRecent_Projects->clear();
    for (int i=0; i<IDE::instance()->recentProjects.size(); i++) {
        QAction* na = ui->menuRecent_Projects->addAction(IDE::instance()->recentProjects[i]);
        na->setData(IDE::instance()->recentProjects[i]);
    }
    ui->menuRecent_Projects->addSeparator();
    ui->menuRecent_Projects->addAction("Clear Menu");
}
void MainWindow::updateRecentFiles(const QString& p) {
    if (!p.isEmpty()) {
        IDE::instance()->addRecentFile(p);
    }
    ui->menuRecent_Files->clear();
    for (int i=0; i<IDE::instance()->recentFiles.size(); i++) {
        QAction* na = ui->menuRecent_Files->addAction(IDE::instance()->recentFiles[i]);
        na->setData(IDE::instance()->recentFiles[i]);
    }
    ui->menuRecent_Files->addSeparator();
    ui->menuRecent_Files->addAction("Clear Menu");
}

void MainWindow::recentFileMenuAction(QAction* a) {
    if (a->text()=="Clear Menu") {
        IDE::instance()->recentFiles.clear();
        updateRecentFiles("");
    } else {
        openFile(a->data().toString());
    }
}

void MainWindow::recentProjectMenuAction(QAction* a) {
    if (a->text()=="Clear Menu") {
        IDE::instance()->recentProjects.clear();
        updateRecentProjects("");
    } else {
        openProject(a->data().toString());
    }
}

void MainWindow::saveProject(const QString& f)
{
//    QString filepath = f;
//    if (filepath.isEmpty()) {
//        filepath = QFileDialog::getSaveFileName(this,"Save project",getLastPath(),"MiniZinc projects (*.mzp)");
//        if (!filepath.isNull()) {
//            setLastPath(QFileInfo(filepath).absolutePath()+fileDialogSuffix);
//        }
//    }
//    if (!filepath.isEmpty()) {
//        if (projectPath != filepath && IDE::instance()->projects.contains(filepath)) {
//            QMessageBox::warning(this,"MiniZinc IDE","Cannot overwrite existing open project.",
//                                 QMessageBox::Ok);
//            return;
//        }
//        QFile file(filepath);
//        if (file.open(QFile::WriteOnly | QFile::Text)) {
//            if (projectPath != filepath) {
//                IDE::instance()->projects.remove(projectPath);
//                IDE::instance()->projects.insert(filepath,this);
//                project.setRoot(ui->projectView, projectSort, filepath);
//                projectPath = filepath;
//            }
//            updateRecentProjects(projectPath);
//            tabChange(ui->tabWidget->currentIndex());

//            QJsonObject confObject;

//            confObject["version"] = 105;

//            QStringList openFiles;
//            QDir projectDir = QFileInfo(filepath).absoluteDir();
//            for (int i=0; i<ui->tabWidget->count(); i++) {
//                CodeEditor* ce = static_cast<CodeEditor*>(ui->tabWidget->widget(i));
//                if (!ce->filepath.isEmpty())
//                    openFiles << projectDir.relativeFilePath(ce->filepath);
//            }
//            confObject["openFiles"] = QJsonArray::fromStringList(openFiles);
//            confObject["openTab"] = ui->tabWidget->currentIndex();

//            QStringList projectFilesRelPath;
//            QStringList projectFiles = project.files();
//            for (QList<QString>::const_iterator it = projectFiles.begin();
//                 it != projectFiles.end(); ++it) {
//                projectFilesRelPath << projectDir.relativeFilePath(*it);
//            }
//            confObject["projectFiles"] = QJsonArray::fromStringList(projectFilesRelPath);

//            // Make sure any configuration changes have been updated
//            on_conf_solver_conf_currentIndexChanged(ui->conf_solver_conf->currentIndex());

//            // Save all project solver configurations
//            QJsonArray projectConfigs;
//            for (int i=0; i<projectSolverConfigs.size(); i++) {
//                SolverConfiguration& sc = projectSolverConfigs[i];
//                QJsonObject projConf;
//                projConf["name"] = sc.name;
//                projConf["id"] = sc.solverId;
//                projConf["version"] = sc.solverVersion;
//                projConf["timeLimit"] = sc.timeLimit;
//                projConf["defaultBehavior"] = sc.defaultBehaviour;
//                projConf["printIntermediate"] = sc.printIntermediate;
//                projConf["stopAfter"] = sc.stopAfter;
//                projConf["compressSolutionOutput"] = sc.compressSolutionOutput;
//                projConf["clearOutputWindow"] = sc.clearOutputWindow;
//                projConf["verboseFlattening"] = sc.verboseFlattening;
//                projConf["flatteningStats"] = sc.flatteningStats;
//                projConf["optimizationLevel"] = sc.optimizationLevel;
//                projConf["additionalData"] = sc.additionalData;
//                projConf["additionalCompilerCommandline"] = sc.additionalCompilerCommandline;
//                projConf["nThreads"] = sc.nThreads;
//                if (sc.randomSeed.isValid()) {
//                    projConf["randomSeed"] = sc.randomSeed.toInt();
//                }
//                projConf["solverFlags"] = sc.solverFlags;
//                projConf["freeSearch"] = sc.freeSearch;
//                projConf["verboseSolving"] = sc.verboseSolving;
//                projConf["outputTiming"] = sc.outputTiming;
//                projConf["solvingStats"] = sc.solvingStats;
//                projConf["runSolutionChecker"] = sc.runSolutionChecker;
//                projConf["useExtraOptions"] = sc.useExtraOptions;
//                if (!sc.extraOptions.empty()) {
//                    QJsonObject extraOptions;
//                    for (auto& k : sc.extraOptions.keys()) {
//                        extraOptions[k] = sc.extraOptions[k];
//                    }
//                    projConf["extraOptions"] = extraOptions;
//                }
//                projectConfigs.append(projConf);
//            }
//            confObject["projectSolverConfigs"] = projectConfigs;
//            // Save all modified built-in solver configurations
//            QJsonArray builtinConfigs;
//            for (int i=0; i<builtinSolverConfigs.size(); i++) {
//                SolverConfiguration& sc = builtinSolverConfigs[i];

//                SolverConfiguration defConf = SolverConfiguration::defaultConfig();
//                defConf.name = sc.name;
//                defConf.solverId = sc.solverId;
//                defConf.solverVersion = sc.solverVersion;
//                if (!(defConf==sc)) {
//                    QJsonObject projConf;
//                    projConf["name"] = sc.name;
//                    projConf["id"] = sc.solverId;
//                    projConf["version"] = sc.solverVersion;
//                    projConf["timeLimit"] = sc.timeLimit;
//                    projConf["defaultBehavior"] = sc.defaultBehaviour;
//                    projConf["printIntermediate"] = sc.printIntermediate;
//                    projConf["stopAfter"] = sc.stopAfter;
//                    projConf["compressSolutionOutput"] = sc.compressSolutionOutput;
//                    projConf["clearOutputWindow"] = sc.clearOutputWindow;
//                    projConf["verboseFlattening"] = sc.verboseFlattening;
//                    projConf["flatteningStats"] = sc.flatteningStats;
//                    projConf["optimizationLevel"] = sc.optimizationLevel;
//                    projConf["additionalData"] = sc.additionalData;
//                    projConf["additionalCompilerCommandline"] = sc.additionalCompilerCommandline;
//                    projConf["nThreads"] = sc.nThreads;
//                    if (sc.randomSeed.isValid()) {
//                        projConf["randomSeed"] = sc.randomSeed.toInt();
//                    }
//                    projConf["solverFlags"] = sc.solverFlags;
//                    projConf["freeSearch"] = sc.freeSearch;
//                    projConf["verboseSolving"] = sc.verboseSolving;
//                    projConf["outputTiming"] = sc.outputTiming;
//                    projConf["solvingStats"] = sc.solvingStats;
//                    projConf["runSolutionChecker"] = sc.runSolutionChecker;
//                    projConf["useExtraOptions"] = sc.useExtraOptions;
//                    if (!sc.extraOptions.empty()) {
//                        QJsonObject extraOptions;
//                        for (auto& k : sc.extraOptions.keys()) {
//                            extraOptions[k] = sc.extraOptions[k];
//                        }
//                        projConf["extraOptions"] = extraOptions;
//                    }
//                    builtinConfigs.append(projConf);
//                }
//            }
//            confObject["builtinSolverConfigs"] = builtinConfigs;

//            int scIdx = ui->conf_solver_conf->currentIndex();
//            if (scIdx < projectSolverConfigs.size()) {
//                // currently selected config has been saved to the project
//                confObject["selectedProjectConfig"] = scIdx;
//            } else {
//                if (projectSolverConfigs.size()!=0)
//                    scIdx--;
//                SolverConfiguration& curSc = builtinSolverConfigs[scIdx-projectSolverConfigs.size()];
//                confObject["selectedBuiltinConfigId"] = curSc.solverId;
//                confObject["selectedBuiltinConfigVersion"] = curSc.solverVersion;
//            }

//            QJsonDocument jdoc(confObject);
//            file.write(jdoc.toJson());
//            file.close();
//        } else {
//            QMessageBox::warning(this,"MiniZinc IDE","Could not save project");
//        }
//    }
}

namespace {
//    SolverConfiguration scFromJson(QJsonObject sco) {
//        SolverConfiguration newSc;
//        if (sco["name"].isString()) {
//            newSc.name = sco["name"].toString();
//        }
//        if (sco["id"].isString()) {
//            newSc.solverId = sco["id"].toString();
//        }
//        if (sco["version"].isString()) {
//            newSc.solverVersion = sco["version"].toString();
//        }
//        if (sco["timeLimit"].isDouble()) {
//            newSc.timeLimit = sco["timeLimit"].toDouble();
//        }
//        if (sco["defaultBehavior"].isBool()) {
//            newSc.defaultBehaviour = sco["defaultBehavior"].toBool();
//        }
//        if (sco["printIntermediate"].isBool()) {
//            newSc.printIntermediate = sco["printIntermediate"].toBool();
//        }
//        if (sco["stopAfter"].isDouble()) {
//            newSc.stopAfter = sco["stopAfter"].toDouble();
//        }
//        if (sco["compressSolutionOutput"].isDouble()) {
//            newSc.compressSolutionOutput = sco["compressSolutionOutput"].toDouble();
//        }
//        if (sco["clearOutputWindow"].isBool()) {
//            newSc.clearOutputWindow = sco["clearOutputWindow"].toBool();
//        }
//        if (sco["verboseFlattening"].isBool()) {
//            newSc.verboseFlattening = sco["verboseFlattening"].toBool();
//        }
//        if (sco["flatteningStats"].isBool()) {
//            newSc.flatteningStats = sco["flatteningStats"].toBool();
//        }
//        if (sco["optimizationLevel"].isDouble()) {
//            newSc.optimizationLevel = sco["optimizationLevel"].toDouble();
//        }
//        if (sco["additionalData"].isString()) {
//            newSc.additionalData = sco["additionalData"].toString();
//        }
//        if (sco["additionalCompilerCommandline"].isString()) {
//            newSc.additionalCompilerCommandline = sco["additionalCompilerCommandline"].toString();
//        }
//        if (sco["nThreads"].isDouble()) {
//            newSc.nThreads = sco["nThreads"].toDouble();
//        }
//        if (sco["randomSeed"].isDouble()) {
//            newSc.randomSeed = sco["randomSeed"].toDouble();
//        }
//        if (sco["solverFlags"].isString()) {
//            newSc.solverFlags = sco["solverFlags"].toString();
//        }
//        if (sco["freeSearch"].isBool()) {
//            newSc.freeSearch = sco["freeSearch"].toBool();
//        }
//        if (sco["verboseSolving"].isBool()) {
//            newSc.verboseSolving = sco["verboseSolving"].toBool();
//        }
//        if (sco["outputTiming"].isBool()) {
//            newSc.outputTiming = sco["outputTiming"].toBool();
//        }
//        if (sco["solvingStats"].isBool()) {
//            newSc.solvingStats = sco["solvingStats"].toBool();
//        }
//        if (sco["runSolutionChecker"].isBool()) {
//            newSc.runSolutionChecker = sco["runSolutionChecker"].toBool();
//        }
//        if (sco["useExtraOptions"].isBool()) {
//            newSc.useExtraOptions = sco["useExtraOptions"].toBool();
//        }
//        if (sco["extraOptions"].isObject()) {
//            QJsonObject extraOptions = sco["extraOptions"].toObject();
//            for (auto& k : extraOptions.keys()) {
//                newSc.extraOptions[k] = extraOptions[k].toString();
//            }
//        }
//        return newSc;
//    }
}

void MainWindow::loadProject(const QString& filepath)
{
//    QFile pfile(filepath);
//    pfile.open(QIODevice::ReadOnly);
//    if (!pfile.isOpen()) {
//        QMessageBox::warning(this, "MiniZinc IDE",
//                             "Could not open project file");
//        return;
//    }
//    QByteArray jsonData = pfile.readAll();
//    QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonData);
//    bool jsonError = false;
//    if (!jsonDoc.isNull()) {
//        QStringList projectFilesRelPath;
//        QString basePath = QFileInfo(filepath).absolutePath()+"/";
//        QStringList openFiles;
//        int openTab = -1;
//        if (jsonDoc.isObject()) {
//            QJsonObject confObject(jsonDoc.object());
//            projectPath = filepath;
//            updateRecentProjects(projectPath);
//            project.setRoot(ui->projectView, projectSort, projectPath);
//            if (confObject["openFiles"].isArray()) {
//                QJsonArray openFilesA(confObject["openFiles"].toArray());
//                for (auto f : openFilesA) {
//                    if (f.isString()) {
//                        openFiles.append(f.toString());
//                    } else {
//                        jsonError = true;
//                        goto errorInJsonConfig;
//                    }
//                }
//            }
//            openTab = confObject["openTab"].isDouble() ? confObject["openTab"].toDouble() : -1;
//            if (confObject["projectFiles"].isArray()) {
//                QJsonArray projectFilesA(confObject["projectFiles"].toArray());
//                for (auto f : projectFilesA) {
//                    if (f.isString()) {
//                        projectFilesRelPath.append(f.toString());
//                    } else {
//                        jsonError = true;
//                        goto errorInJsonConfig;
//                    }
//                }
//            }
//            // Load project solver configurations
//            if (confObject["projectSolverConfigs"].isArray()) {
//                QJsonArray solverConfA(confObject["projectSolverConfigs"].toArray());
//                for (auto sc : solverConfA) {
//                    if (sc.isObject()) {
//                        QJsonObject sco(sc.toObject());
//                        SolverConfiguration newSc = scFromJson(sco);
//                        projectSolverConfigs.push_back(newSc);
//                    } else {
//                        jsonError = true;
//                        goto errorInJsonConfig;
//                    }
//                }
//            }
//            project.solverConfigs(projectSolverConfigs,true);
//            // Load built-in solver configurations
//            if (confObject["builtinSolverConfigs"].isArray()) {
//                QJsonArray solverConfA(confObject["builtinSolverConfigs"].toArray());
//                for (auto sc : solverConfA) {
//                    if (sc.isObject()) {
//                        QJsonObject sco(sc.toObject());
//                        SolverConfiguration newSc = scFromJson(sco);
//                        int updateSc = -1;
//                        for (int i=0; i<builtinSolverConfigs.size(); i++) {
//                            if (newSc.solverId==builtinSolverConfigs[i].solverId) {
//                                updateSc = i;
//                                if (newSc.solverVersion==builtinSolverConfigs[i].solverVersion) {
//                                    builtinSolverConfigs[i] = newSc;
//                                    updateSc = -1;
//                                    break;
//                                }
//                            }
//                        }
//                        if (updateSc != -1) {
//                            builtinSolverConfigs[updateSc] = newSc;
//                        }
//                    } else {
//                        jsonError = true;
//                        goto errorInJsonConfig;
//                    }
//                }
//            }
//            updateSolverConfigs();
//            if (confObject["selectedProjectConfig"].isDouble()) {
//                int selected = confObject["selectedProjectConfig"].toDouble();
//                if (selected < projectSolverConfigs.size())
//                    setCurrentSolverConfig(selected);
//            } else {
//                if (confObject["selectedBuiltinConfigId"].isString() && confObject["selectedBuiltinConfigVersion"].isString()) {
//                    int selectConfig = 0;
//                    QString selId = confObject["selectedBuiltinConfigId"].toString();
//                    QString selVer = confObject["selectedBuiltinConfigVersion"].toString();
//                    for (int i=0; i<builtinSolverConfigs.size(); i++) {
//                        if (selId==builtinSolverConfigs[i].solverId) {
//                            selectConfig = i;
//                            if (selVer==builtinSolverConfigs[i].solverVersion)
//                                break;
//                        }
//                    }
//                    setCurrentSolverConfig(projectSolverConfigs.size()+selectConfig);
//                } else {
//                    jsonError = true;
//                }
//            }
//        } else {
//            jsonError = true;
//        }
//      errorInJsonConfig:
//        if (jsonError) {
//            QMessageBox::warning(this, "MiniZinc IDE",
//                                 "Error in project file");
//        } else {
//            QStringList missingFiles;
//            for (int i=0; i<projectFilesRelPath.size(); i++) {
//                QFileInfo fi(basePath+projectFilesRelPath[i]);
//                if (fi.exists()) {
//                    project.addFile(ui->projectView, projectSort, basePath+projectFilesRelPath[i]);
//                } else {
//                    missingFiles.append(basePath+projectFilesRelPath[i]);
//                }
//            }
//            if (!missingFiles.empty()) {
//                QMessageBox::warning(this, "MiniZinc IDE", "Could not find files in project:\n"+missingFiles.join("\n"));
//            }

//            for (int i=0; i<openFiles.size(); i++) {
//                QFileInfo fi(basePath+openFiles[i]);
//                if (fi.exists()) {
//                    openFile(basePath+openFiles[i],false);
//                }
//            }

//            project.setModified(false, true);

//            IDE::instance()->projects.insert(projectPath, this);
//            ui->tabWidget->setCurrentIndex(openTab != -1 ? openTab : ui->tabWidget->currentIndex());
//            if (ui->projectExplorerDockWidget->isHidden()) {
//                on_actionShow_project_explorer_triggered();
//            }

//        }
//        return;

//    } else {
//        pfile.reset();
//        QDataStream in(&pfile);
//        quint32 magic;
//        in >> magic;
//        if (magic != 0xD539EA12) {
//            QMessageBox::warning(this, "MiniZinc IDE",
//                                 "Could not open project file");
//            close();
//            return;
//        }
//        quint32 version;
//        in >> version;
//        if (version != 101 && version != 102 && version != 103 && version != 104) {
//            QMessageBox::warning(this, "MiniZinc IDE",
//                                 "Could not open project file (version mismatch)");
//            close();
//            return;
//        }
//        in.setVersion(QDataStream::Qt_5_0);

//        projectPath = filepath;
//        updateRecentProjects(projectPath);
//        project.setRoot(ui->projectView, projectSort, projectPath);
//        QString basePath;
//        if (version==103 || version==104) {
//            basePath = QFileInfo(filepath).absolutePath()+"/";
//        }

//        QStringList openFiles;
//        in >> openFiles;
//        QString p_s;
//        bool p_b;

//        int dataFileIndex;

//        SolverConfiguration newConf;
//        newConf.isBuiltin = false;
//        newConf.runSolutionChecker = true;

//        in >> p_s; // Used to be additional include path
//        in >> dataFileIndex;
//        in >> p_b;
//        // Ignore, not used any longer
//    //    project.haveExtraArgs(p_b, true);
//        in >> newConf.additionalData;
//        in >> p_b;
//        // Ignore, not used any longer
//        in >> newConf.additionalCompilerCommandline;
//        if (version==104) {
//            in >> newConf.clearOutputWindow;
//        } else {
//            newConf.clearOutputWindow = false;
//        }
//        in >> newConf.verboseFlattening;
//        in >> p_b;
//        newConf.optimizationLevel = p_b ? 1 : 0;
//        in >> p_s; // Used to be solver name
//        in >> newConf.stopAfter;
//        in >> newConf.printIntermediate;
//        in >> newConf.solvingStats;
//        in >> p_b;
//        // Ignore
//        in >> newConf.solverFlags;
//        in >> newConf.nThreads;
//        in >> p_b;
//        in >> p_s;
//        newConf.randomSeed = p_b ? QVariant::fromValue(p_s.toInt()) : QVariant();
//        in >> p_b; // used to be whether time limit is checked
//        in >> newConf.timeLimit;
//        int openTab = -1;
//        if (version==102 || version==103 || version==104) {
//            in >> newConf.verboseSolving;
//            in >> openTab;
//        }
//        QStringList projectFilesRelPath;
//        if (version==103 || version==104) {
//            in >> projectFilesRelPath;
//        } else {
//            projectFilesRelPath = openFiles;
//        }
//        if ( (version==103 || version==104) && !in.atEnd()) {
//            in >> newConf.defaultBehaviour;
//        } else {
//            newConf.defaultBehaviour = (newConf.stopAfter == 1 && !newConf.printIntermediate);
//        }
//        if (version==104 && !in.atEnd()) {
//            in >> newConf.flatteningStats;
//        }
//        if (version==104 && !in.atEnd()) {
//            in >> newConf.compressSolutionOutput;
//        }
//        if (version==104 && !in.atEnd()) {
//            in >> newConf.outputTiming;
//        }
//        // create new solver configuration based on projet settings
//        newConf.solverId = builtinSolverConfigs[defaultSolverIdx].solverId;
//        newConf.solverVersion = builtinSolverConfigs[defaultSolverIdx].solverVersion;
//        bool foundConfig = false;
//        for (int i=0; i<builtinSolverConfigs.size(); i++) {
//            if (builtinSolverConfigs[i]==newConf) {
//                setCurrentSolverConfig(i);
//                foundConfig = true;
//                break;
//            }
//        }
//        if (!foundConfig) {
//            newConf.name = "Project solver configuration";
//            projectSolverConfigs.push_front(newConf);
//            currentSolverConfig = 0;
//            project.solverConfigs(projectSolverConfigs,true);
//            updateSolverConfigs();
//            setCurrentSolverConfig(0);
//        }
//        QStringList missingFiles;
//        for (int i=0; i<projectFilesRelPath.size(); i++) {
//            QFileInfo fi(basePath+projectFilesRelPath[i]);
//            if (fi.exists()) {
//                project.addFile(ui->projectView, projectSort, basePath+projectFilesRelPath[i]);
//            } else {
//                missingFiles.append(basePath+projectFilesRelPath[i]);
//            }
//        }
//        if (!missingFiles.empty()) {
//            QMessageBox::warning(this, "MiniZinc IDE", "Could not find files in project:\n"+missingFiles.join("\n"));
//        }

//        for (int i=0; i<openFiles.size(); i++) {
//            QFileInfo fi(basePath+openFiles[i]);
//            if (fi.exists()) {
//                openFile(basePath+openFiles[i],false);
//            }
//        }

//        project.setModified(false, true);
//        ui->tabWidget->setCurrentIndex(openTab != -1 ? openTab : ui->tabWidget->currentIndex());
//        IDE::instance()->projects.insert(projectPath, this);
//        if (ui->projectExplorerDockWidget->isHidden()) {
//            on_actionShow_project_explorer_triggered();
//        }
//    }
}

void MainWindow::on_actionSave_project_triggered()
{
    saveProject(projectPath);
}

void MainWindow::on_actionSave_project_as_triggered()
{
    saveProject(QString());
}

void MainWindow::on_actionClose_project_triggered()
{
    close();
}

void MainWindow::on_actionFind_next_triggered()
{
    on_b_next_clicked();
}

void MainWindow::on_actionFind_previous_triggered()
{
    on_b_prev_clicked();
}

void MainWindow::on_actionSave_all_triggered()
{
    for (int i=0; i<ui->tabWidget->count(); i++) {
        CodeEditor* ce = static_cast<CodeEditor*>(ui->tabWidget->widget(i));
        if (ce->document()->isModified())
            saveFile(ce,ce->filepath);
    }
}

void MainWindow::on_action_Un_comment_triggered()
{
    if (curEditor==nullptr)
        return;
    QTextCursor cursor = curEditor->textCursor();
    QTextBlock beginBlock = curEditor->document()->findBlock(cursor.anchor());
    QTextBlock endblock = curEditor->document()->findBlock(cursor.position());
    if (beginBlock.blockNumber() > endblock.blockNumber())
        std::swap(beginBlock,endblock);
    endblock = endblock.next();

    QRegExp comment("^(\\s*%|\\s*$)");
    QRegExp comSpace("%\\s");
    QRegExp emptyLine("^\\s*$");

    QTextBlock block = beginBlock;
    bool isCommented = true;
    do {
        if (comment.indexIn(block.text()) == -1) {
            isCommented = false;
            break;
        }
        block = block.next();
    } while (block.isValid() && block != endblock);

    block = beginBlock;
    cursor.beginEditBlock();
    do {
        cursor.setPosition(block.position());
        QString t = block.text();
        if (isCommented) {
            int cpos = t.indexOf("%");
            if (cpos != -1) {
                cursor.setPosition(block.position()+cpos);
                bool haveSpace = (comSpace.indexIn(t,cpos)==cpos);
                cursor.movePosition(QTextCursor::Right,QTextCursor::KeepAnchor,haveSpace ? 2:1);
                cursor.removeSelectedText();
            }

        } else {
            if (emptyLine.indexIn(t)==-1)
                cursor.insertText("% ");
        }
        block = block.next();
    } while (block.isValid() && block != endblock);
    cursor.endEditBlock();
}

void MainWindow::on_actionOnly_editor_triggered()
{
    if (!ui->outputDockWidget->isFloating())
        ui->outputDockWidget->hide();
}

void MainWindow::on_actionSplit_triggered()
{
    if (!ui->outputDockWidget->isFloating())
        ui->outputDockWidget->show();
}

void MainWindow::on_actionPrevious_tab_triggered()
{
    if (ui->tabWidget->currentIndex() > 0) {
        ui->tabWidget->setCurrentIndex(ui->tabWidget->currentIndex()-1);
    } else {
        ui->tabWidget->setCurrentIndex(ui->tabWidget->count()-1);
    }
}

void MainWindow::on_actionNext_tab_triggered()
{
    if (ui->tabWidget->currentIndex() < ui->tabWidget->count()-1) {
        ui->tabWidget->setCurrentIndex(ui->tabWidget->currentIndex()+1);
    } else {
        ui->tabWidget->setCurrentIndex(0);
    }
}

void MainWindow::on_actionHide_tool_bar_triggered()
{
    if (ui->toolBar->isHidden()) {
        ui->toolBar->show();
        ui->actionHide_tool_bar->setText("Hide tool bar");
    } else {
        ui->toolBar->hide();
        ui->actionHide_tool_bar->setText("Show tool bar");
    }
}

void MainWindow::on_actionToggle_profiler_info_triggered()
{
    profileInfoVisible = !profileInfoVisible;
    for (int i=0; i<ui->tabWidget->count(); i++) {
        CodeEditor* ce = static_cast<CodeEditor*>(ui->tabWidget->widget(i));
        ce->showDebugInfo(profileInfoVisible);
    }
}

void MainWindow::on_actionShow_project_explorer_triggered()
{
    if (ui->projectExplorerDockWidget->isHidden()) {
        ui->projectExplorerDockWidget->show();
        ui->actionShow_project_explorer->setText("Hide project explorer");
    } else {
        ui->projectExplorerDockWidget->hide();
        ui->actionShow_project_explorer->setText("Show project explorer");
    }
}

void MainWindow::onClipboardChanged()
{
    ui->actionPaste->setEnabled(!QApplication::clipboard()->text().isEmpty());
}

void MainWindow::editor_cursor_position_changed()
{
    if (curEditor) {
        statusLineColLabel->setText(QString("Line: ")+QString().number(curEditor->textCursor().blockNumber()+1)+", Col: "+QString().number(curEditor->textCursor().columnNumber()+1));
    }
}

void MainWindow::on_actionSubmit_to_MOOC_triggered()
{
    // Check if any documents need saving
    QVector<CodeEditor*> modifiedDocs;
    QSet<QString> files;
    for (auto problem : project.moocAssignment().problems) {
        files.insert(problem.model);
        files.insert(problem.data);
    }
    for (int i = 0; i < ui->tabWidget->count(); i++) {
        auto ce = static_cast<CodeEditor*>(ui->tabWidget->widget(i));
        if (ce->document()->isModified() && files.contains(ce->filepath)) {
            modifiedDocs.append(ce);
        }
    }
    if (!modifiedDocs.empty()) {
        if (!saveBeforeRunning) {
            QMessageBox msgBox;
            if (modifiedDocs.size()==1) {
                msgBox.setText("One of the files has been modified. You must save it before submitting.");
            } else {
                msgBox.setText("Several files have been modified. You must save them before submitting.");
            }
            msgBox.setInformativeText("Do you want to save now and then submit?");
            QAbstractButton *saveButton = msgBox.addButton(QMessageBox::Save);
            msgBox.addButton(QMessageBox::Cancel);
            QAbstractButton *alwaysButton = msgBox.addButton("Always save", QMessageBox::AcceptRole);
            msgBox.setDefaultButton(QMessageBox::Save);
            msgBox.exec();
            if (msgBox.clickedButton() == alwaysButton) {
                saveBeforeRunning = true;
            }
            else if (msgBox.clickedButton() != saveButton) {
                return;
            }
        }
        for (auto ce : modifiedDocs) {
            saveFile(ce,ce->filepath);
            if (ce->document()->isModified()) {
                return;
            }
        }
    }

    moocSubmission = new MOOCSubmission(this, project.moocAssignment());
    connect(moocSubmission, SIGNAL(finished(int)), this, SLOT(moocFinished(int)));
    setEnabled(false);
    moocSubmission->show();
}

void MainWindow::moocFinished(int) {
    moocSubmission->deleteLater();
    setEnabled(true);
}

bool MainWindow::eventFilter(QObject *obj, QEvent *ev)
{
    if (obj == ui->outputConsole) {
        if (ev->type() == QEvent::KeyPress) {
            QKeyEvent *keyEvent = static_cast<QKeyEvent*>(ev);
            if (keyEvent == QKeySequence::Copy) {
                ui->outputConsole->copy();
                return true;
            } else if (keyEvent == QKeySequence::Cut) {
                ui->outputConsole->cut();
                return true;
            }
        }
        return false;
    } else {
        return QMainWindow::eventFilter(obj,ev);
    }
}

void MainWindow::on_actionCheat_Sheet_triggered()
{
    IDE::instance()->cheatSheet->show();
    IDE::instance()->cheatSheet->raise();
    IDE::instance()->cheatSheet->activateWindow();
}

void MainWindow::check_code()
{
    auto sc = getCurrentSolverConfig();
    if (!MznDriver::get().isValid() || !ui->actionRun->isEnabled() ||
            !curEditor || (!curEditor->filepath.isEmpty() && !curEditor->filepath.endsWith(".mzn")) ||
            !sc) {
        return;
    }
    auto contents = curEditor->document()->toPlainText();
    auto wd = QFileInfo(curEditor->filepath).absolutePath();
    if (code_checker) {
        code_checker->disconnect();
        code_checker->cancel();
        code_checker->deleteLater();
    }
    code_checker = new CodeChecker(this);
    connect(code_checker, &CodeChecker::finished, this, [=](const QVector<MiniZincError>& mznErrors) {
        code_checker->deleteLater();
        code_checker = nullptr;
        if (curEditor) {
            curEditor->checkFile(mznErrors);
        }
    });
    code_checker->start(contents, *sc, wd);
}

void MainWindow::on_actionDark_mode_toggled(bool enable)
{
    darkMode = enable;
    QSettings settings;
    settings.beginGroup("MainWindow");
    settings.setValue("darkMode",darkMode);
    settings.endGroup();
    for (int i=0; i<ui->tabWidget->count(); i++) {
        CodeEditor* ce = static_cast<CodeEditor*>(ui->tabWidget->widget(i));
        ce->setDarkMode(darkMode);
    }
    static_cast<CodeEditor*>(IDE::instance()->cheatSheet->centralWidget())->setDarkMode(darkMode);

    if (darkMode) {
#ifdef Q_OS_WIN
        QFile sheet(":/dark_mode.css");
        sheet.open(QFile::ReadOnly);
        qApp->setStyleSheet(sheet.readAll());
#endif
        ui->outputConsole->document()->setDefaultStyleSheet(".mznnotice { color : #13C4F5 }");
    } else {
#ifdef Q_OS_WIN
        qApp->setStyleSheet("");
#endif
        ui->outputConsole->document()->setDefaultStyleSheet(".mznnotice { color : blue }");
    }
}

void MainWindow::on_actionEditSolverConfig_triggered()
{
    if (ui->configWindow_dockWidget->isHidden()) {
        ui->configWindow_dockWidget->show();
    } else {
        ui->configWindow_dockWidget->hide();
    }
}

void MainWindow::on_b_next_clicked()
{
    find(true);
    incrementalFindCursor.setPosition(std::min(curEditor->textCursor().anchor(), curEditor->textCursor().position()));
}

void MainWindow::on_b_prev_clicked()
{
    find(false);
    incrementalFindCursor.setPosition(std::min(curEditor->textCursor().anchor(), curEditor->textCursor().position()));
}

void MainWindow::on_b_replacefind_clicked()
{
    on_b_replace_clicked();
    find(true);
    incrementalFindCursor.setPosition(std::min(curEditor->textCursor().anchor(), curEditor->textCursor().position()));
}

void MainWindow::on_b_replace_clicked()
{
    QTextCursor cursor = curEditor->textCursor();
    if (cursor.hasSelection()) {
        cursor.insertText(ui->replace->text());
    }
}

void MainWindow::on_b_replaceall_clicked()
{
    int counter = 0;
    QTextCursor cursor = curEditor->textCursor();
    QTextCursor origCursor = curEditor->textCursor();
    cursor.movePosition(QTextCursor::Start);
    curEditor->setTextCursor(cursor);
    if (!cursor.hasSelection()) {
        find(true,true);
        cursor = curEditor->textCursor();
    }
    cursor.beginEditBlock();
    while (cursor.hasSelection()) {
        counter++;
        cursor.insertText(ui->replace->text());
        find(true,true);
        cursor = curEditor->textCursor();
    }
    cursor.endEditBlock();
    if (counter > 0) {
        ui->not_found->setText(QString().number(counter)+" replaced");
    } else {
        curEditor->setTextCursor(origCursor);
    }
    incrementalFindCursor.setPosition(std::min(curEditor->textCursor().anchor(), curEditor->textCursor().position()));
}

void MainWindow::on_closeFindWidget_clicked()
{
    ui->findFrame->hide();
    curEditor->setFocus();
}

void MainWindow::on_find_textEdited(const QString &)
{
    curEditor->setTextCursor(incrementalFindCursor);
    find(true);
}

void MainWindow::on_actionProfile_compilation_triggered()
{
    compileOrRun(CM_PROFILE);
    if (!profileInfoVisible) {
        on_actionToggle_profiler_info_triggered();
    }
}

void MainWindow::setEditorMenuItemsEnabled(bool enabled) {
    // These items only need to be enabled when there is an editor tab open
    ui->actionClose->setEnabled(enabled);
    ui->menuFind->setEnabled(enabled);
    ui->actionFind->setEnabled(enabled);
    ui->actionReplace->setEnabled(enabled);
    ui->actionFind_next->setEnabled(enabled);
    ui->actionFind_previous->setEnabled(enabled);
    ui->actionShift_left->setEnabled(enabled);
    ui->actionShift_right->setEnabled(enabled);
    ui->actionCut->setEnabled(enabled);
    ui->actionCopy->setEnabled(enabled);
    ui->actionPaste->setEnabled(enabled);
    ui->actionSelect_All->setEnabled(enabled);
    ui->actionGo_to_line->setEnabled(enabled);
    ui->action_Un_comment->setEnabled(enabled);
    ui->actionSave->setEnabled(enabled);
    ui->actionSave_as->setEnabled(enabled);
    ui->actionSave_all->setEnabled(enabled);
    ui->actionUndo->setEnabled(enabled);
    ui->actionRedo->setEnabled(enabled);
    ui->actionRun->setEnabled(enabled);
}

void MainWindow::on_configWindow_dockWidget_visibilityChanged(bool visible)
{
    if (visible) {
        ui->actionEditSolverConfig->setText("Hide configuration editor...");
    } else {
        ui->actionEditSolverConfig->setText("Show configuration editor...");
    }
}

void MainWindow::on_config_window_itemsChanged(const QStringList& items)
{
    disconnect(solverConfCombo, qOverload<int>(&QComboBox::currentIndexChanged), ui->config_window, &ConfigWindow::setCurrentIndex);
    int oldIndex = solverConfCombo->currentIndex();
    solverConfCombo->clear();
    solverConfCombo->addItems(items);
    solverConfCombo->setCurrentIndex(oldIndex);
    connect(solverConfCombo, qOverload<int>(&QComboBox::currentIndexChanged), ui->config_window, &ConfigWindow::setCurrentIndex);
}

void MainWindow::on_config_window_selectedIndexChanged(int index)
{
    solverConfCombo->setCurrentIndex(index);
}

SolverConfiguration* MainWindow::getCurrentSolverConfig()
{
    return ui->config_window->currentSolverConfig();
}

const Solver* MainWindow::getCurrentSolver()
{
    auto sc = getCurrentSolverConfig();
    if (!sc) {
        return nullptr;
    }
    return &sc->solverDefinition;
}

bool MainWindow::requireMiniZinc()
{
    if (!MznDriver::get().isValid()) {
        int ret = QMessageBox::warning(this,
                                       "MiniZinc IDE",
                                       "Could not find the minizinc executable.\nDo you want to open the solver settings dialog?",
                                       QMessageBox::Ok | QMessageBox::Cancel);
        if (ret == QMessageBox::Ok) {
            on_actionManage_solvers_triggered();
        }
        return false;
    }
    return true;
}

void MainWindow::outputStdErr(const QString& l)
{
    QRegExp errexp("^(.*):(([0-9]+)(\\.([0-9]+)(-[0-9]+(\\.[0-9]+)?)?)?):\\s*$");
    if (errexp.indexIn(l) != -1) {
        QString errFile = errexp.cap(1).trimmed();
        if (errFile.endsWith("untitled_model.mzn")) {
            QFileInfo errFileInfo(errFile);
            for (QTemporaryDir* d : cleanupTmpDirs) {
                QFileInfo tmpFileInfo(d->path()+"/untitled_model.mzn");
                if (errFileInfo.canonicalFilePath()==tmpFileInfo.canonicalFilePath()) {
                    errFile = "Playground";
                    break;
                }
            }
        }
        QUrl url = QUrl::fromLocalFile(errFile);
        QString query = "line="+errexp.cap(3);
        if (!errexp.cap(5).isEmpty())
            query += "&column="+errexp.cap(5);
        url.setQuery(query);
        url.setScheme("err");
        IDE::instance()->stats.errorsShown++;
        addOutput("<a style='color:red' href='"+url.toString()+"'>"+errFile+":"+errexp.cap(2)+":</a>");
    } else {
        addOutput(l,false);
    }
}

void MainWindow::on_moocButtonChanged(bool visible, const QString& label, const QIcon& icon)
{
    ui->actionSubmit_to_MOOC->setVisible(visible);
    ui->actionSubmit_to_MOOC->setText(label);
    ui->actionSubmit_to_MOOC->setIcon(icon);
}
