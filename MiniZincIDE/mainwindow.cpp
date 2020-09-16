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

#include "../cp-profiler/src/cpprofiler/execution.hh"
#include "../cp-profiler/src/cpprofiler/options.hh"

#include <QtGlobal>

#define BUILD_YEAR  (__DATE__ + 7)

MainWindow::MainWindow(const QString& project) :
    ui(new Ui::MainWindow),
    curEditor(nullptr),
    curHtmlWindow(-1),
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
    solverConfCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    solverConfCombo->setMinimumWidth(100);
    QLabel* solverConfLabel = new QLabel("Solver configuration:");
    QFont solverConfLabelFont = solverConfLabel->font();
    solverConfLabelFont.setPointSizeF(solverConfLabelFont.pointSizeF()*0.9);
    solverConfLabel->setFont(solverConfLabelFont);
    solverConfFrameLayout->addWidget(solverConfLabel);
    solverConfFrameLayout->addWidget(solverConfCombo);
    solverConfFrame->setLayout(solverConfFrameLayout);
    QAction* solverConfComboAction = ui->toolBar->insertWidget(ui->actionSubmit_to_MOOC, solverConfFrame);

    auto runMenu = new QMenu(this);
    runMenu->addAction(ui->actionRun);
    runMenu->addAction(ui->actionCompile);
    runMenu->addAction(ui->actionProfile_compilation);
    runMenu->addAction(ui->actionProfile_search);

    runButton = new QToolButton;
    runButton->setDefaultAction(ui->actionRun);
    runButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    runButton->setPopupMode(QToolButton::MenuButtonPopup);
    runButton->setMenu(runMenu);

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

    ui->projectExplorerDockWidget->hide();
//    ui->conf_dock_widget->hide();

    project = new Project(ui->config_window->solverConfigs(), this);
    connect(ui->tabWidget, &QTabWidget::currentChanged, [=] (int i) {
        project->openTabsChanged(getOpenFiles(), i);
    });
    connect(ui->config_window, &ConfigWindow::selectedSolverConfigurationChanged, project, &Project::activeSolverConfigChanged);
    connect(ui->config_window, &ConfigWindow::configSaved, [=] (const QString& f) { openFile(f); });
    ui->projectBrowser->project(project);

    connect(project, &Project::moocChanged, this, &MainWindow::on_moocChanged);

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

    getProject().setModified(false);

    ui->cpprofiler_dockWidget->hide();
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

MainWindow::~MainWindow()
{
    for (int i=0; i<cleanupTmpDirs.size(); i++) {
        delete cleanupTmpDirs[i];
    }
    for (int i=0; i<cleanupProcesses.size(); i++) {
        cleanupProcesses[i]->terminate();
        delete cleanupProcesses[i];
    }
    delete project;
    delete ui;
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
            CodeEditor* ce = qobject_cast<CodeEditor*>(ui->tabWidget->widget(0));
            if (ce && ce->filepath == "" && !ce->document()->isModified()) {
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
            getProject().add(absPath);
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
            getProject().add(absPath);
            ui->configWindow_dockWidget->setVisible(true);
        } else {
            createEditor(fileName, openAsModified, false, false, focus);
        }
    }

}

void MainWindow::tabCloseRequest(int tab)
{
    CodeEditor* ce = qobject_cast<CodeEditor*>(ui->tabWidget->widget(tab));
    if (ce) {
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
        if (!ce->filepath.isEmpty()) {
            IDE::instance()->removeEditor(ce->filepath,ce);
        }
        if (ce == curEditor) {
            curEditor = nullptr;
        }
        ce->deleteLater();
    } else {
        ui->tabWidget->removeTab(tab);
    }
    getProject().openTabsChanged(getOpenFiles(), ui->tabWidget->currentIndex());
}

void MainWindow::closeEvent(QCloseEvent* e) {
    // make sure any modifications in solver configurations are saved
//    on_conf_solver_conf_currentIndexChanged(ui->conf_solver_conf->currentIndex());

    bool modified = false;
    for (int i=0; i<ui->tabWidget->count(); i++) {
        auto ce = qobject_cast<CodeEditor*>(ui->tabWidget->widget(i));
        if (ce && ce->document()->isModified()) {
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
    for (auto& sc : ui->config_window->solverConfigs()) {
        if (sc->modified) {
            int ret = QMessageBox::warning(this, "MiniZinc IDE",
                                           "There are modified solver configurations.\nDo you want to discard the changes or cancel?",
                                           QMessageBox::Discard | QMessageBox::Cancel);
            if (ret == QMessageBox::Cancel) {
                e->ignore();
                return;
            }
            break;
        }
    }
    if (getProject().isModified()) {
        int ret = QMessageBox::warning(this, "MiniZinc IDE",
                                       "The project has been modified.\nDo you want to discard the changes or cancel?",
                                       QMessageBox::Discard | QMessageBox::Cancel);
        if (ret == QMessageBox::Cancel) {
            e->ignore();
            return;
        }
    }
    if (processRunning) {
        int ret = QMessageBox::warning(this, "MiniZinc IDE",
                                       "MiniZinc is currently running a solver.\nDo you want to quit anyway and stop the current process?",
                                       QMessageBox::Yes| QMessageBox::No);
        if (ret == QMessageBox::No) {
            e->ignore();
            return;
        }
    }

    for (int i=0; i<ui->tabWidget->count(); i++) {
        CodeEditor* ce = qobject_cast<CodeEditor*>(ui->tabWidget->widget(i));
        if (ce) {
            ce->setDocument(nullptr);
            ce->filepath = "";
//            if (ce->filepath != "") { // TODO: How is this possible?
//                IDE::instance()->removeEditor(ce->filepath,ce);
//            }
        }
    }
    if (!projectPath.isEmpty())
        IDE::instance()->projects.remove(projectPath);
    projectPath = "";

    IDE::instance()->mainWindows.remove(this);

    // Stop running subprocesses
    if (code_checker) {
        code_checker->disconnect();
        code_checker->cancel();
        code_checker->deleteLater();
    }
    stop(); // Stop solver if one is running

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
    curEditor = tab == -1 ? nullptr : qobject_cast<CodeEditor*>(ui->tabWidget->widget(tab));
    if (!curEditor) {
        setEditorMenuItemsEnabled(false);
        ui->findFrame->hide();
    } else {
        setEditorMenuItemsEnabled(true);
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
                haveChecker = getProject().contains(checkFile) || getProject().contains(checkFile+".mzn");
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

    updateProfileSearchButton();
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

void MainWindow::compileOrRun(
        CompileMode cm,
        const SolverConfiguration* sc,
        const QString& model,
        const QStringList& data,
        const QString& checker,
        const QStringList& extraArgs)
{
    if (!promptSaveModified()) {
        return;
    }

    // Use either the provided solver config, or the currently active one
    auto solverConfig = sc ? sc : getCurrentSolverConfig();
    if (!solverConfig) {
        // There is no solver config
        return;
    }

    // Use either the model given, or the currently opened model, or prompt for one
    auto modelFile = model.isEmpty() ? currentModelFile() : model;
    if (modelFile.isEmpty()) {
        // There is no model file at all
        return;
    }

    auto dataFiles = data;
    auto checkerFile = checker;
    auto extraArguments = extraArgs;

    // If we didn't specify any data but the current file is a data file, use it
    if (dataFiles.isEmpty() && curEditor && (curEditor->filepath.endsWith(".dzn") || curEditor->filepath.endsWith(".json"))) {
        dataFiles << curEditor->filepath;
    }

    // Prompt for data if necessary
    if (!getModelParameters(*solverConfig, modelFile, dataFiles, extraArguments)) {
        return;
    }

    // If we didn't specify a checker but the current file is one, use it
    if (checkerFile.isEmpty() && curEditor && (curEditor->filepath.endsWith(".mzc") || curEditor->filepath.endsWith(".mzc.mzn"))) {
        checkerFile = curEditor->filepath;
    }

    // Use checker that matches model file if it exists and checking is on
    QSettings settings;
    settings.beginGroup("ide");
    if (checkerFile.isEmpty() && settings.value("checkSolutions", true).toBool()) {
        auto mzc = modelFile;
        mzc.replace(mzc.length() - 1, 1, "c");
        if (getProject().contains(mzc)) {
            checkerFile = mzc;
        } else if (getProject().contains(mzc + ".mzn")) {
            checkerFile = mzc + ".mzn";
        }
    }
    settings.endGroup();

    if (!checkerFile.isEmpty()) {
        dataFiles << checkerFile;
    }

    // Compile/run
    switch (cm) {
    case CM_RUN:
        run(*solverConfig, modelFile, dataFiles, extraArguments);
        break;
    case CM_COMPILE:
        compile(*solverConfig, modelFile, dataFiles, extraArguments);
        break;
    case CM_PROFILE:
        compile(*solverConfig, modelFile, dataFiles, extraArguments, true);
        break;
    }
}

bool MainWindow::getModelParameters(const SolverConfiguration& sc, const QString& model, QStringList& data, QStringList& extraArgs)
{
    if (!requireMiniZinc()) {
        return false;
    }

    // Get model interface to obtain parameters
    QStringList args;
    args << "--model-interface-only" << data << model << extraArgs;

    MznProcess p;
    p.run(sc, args, QFileInfo(model).absolutePath());

    if (!p.waitForStarted() || !p.waitForFinished()) {
        auto message = p.readAllStandardError();
        if (!message.isEmpty()) {
            addOutput(message,false);
        }
        if (p.error() == QProcess::FailedToStart) {
            QMessageBox::critical(this, "MiniZinc IDE", "Failed to start MiniZinc. Check your path settings.");
        } else {
            QMessageBox::critical(this, "MiniZinc IDE", "Unknown error while executing MiniZinc: error code "+ QString().number(p.error()));
        }
        return false;
    }

    QStringList additionalDataFiles;
    QStringList additionalArgs;
    if (p.exitCode() == 0 && p.exitStatus() == QProcess::NormalExit) {
        auto jdoc = QJsonDocument::fromJson(p.readAllStandardOutput());
        if (jdoc.isObject() && jdoc.object()["input"].isObject() && jdoc.object()["method"].isString()) {
            QJsonObject inputArgs = jdoc.object()["input"].toObject();
            QStringList undefinedArgs = inputArgs.keys();
            if (undefinedArgs.size() > 0) {
                QStringList params;
                paramDialog->getParams(undefinedArgs, getProject().dataFiles(), params, additionalDataFiles);
                if (additionalDataFiles.isEmpty()) {
                    if (params.size()==0) {
                        return false;
                    }
                    for (int i=0; i<undefinedArgs.size(); i++) {
                        if (params[i].isEmpty()) {
                            if (! (inputArgs[undefinedArgs[i]].isObject() && inputArgs[undefinedArgs[i]].toObject().contains("optional")) ) {
                                QMessageBox::critical(this, "Undefined parameter", "The parameter '" + undefinedArgs[i] + "' is undefined.");
                                return false;
                            }
                        } else {
                            additionalArgs << "-D" << undefinedArgs[i] + "=" + params[i] + ";";
                        }
                    }
                }
            }
        } else {
            addOutput("<p style='color:red'>Error when checking model parameters:</p>");
            addOutput(p.readAllStandardError(), false);
            QMessageBox::critical(this, "Internal error", "Could not determine model parameters");
            return false;
        }
    }
    data << additionalDataFiles;
    extraArgs << additionalArgs;
    return true;
}

QString MainWindow::currentModelFile()
{
    if (curEditor) {
        if (curEditor->filepath.endsWith(".mzn") && !curEditor->filepath.endsWith(".mzc.mzn")) {
            // The current file is a model
            return curEditor->filepath;
        }
        if (curEditor->filepath.isEmpty()) {
            // The current file is a playground buffer
            QTemporaryDir* modelTmpDir = new QTemporaryDir;
            if (!modelTmpDir->isValid()) {
                throw InternalError("Could not create temporary directory for compilation.");
            } else {
                cleanupTmpDirs.append(modelTmpDir);
                QString model = modelTmpDir->path() +"/untitled_model.mzn";
                QFile modelFile(model);
                if (modelFile.open(QIODevice::ReadWrite)) {
                    QTextStream ts(&modelFile);
                    ts.setCodec("UTF-8");
                    ts << curEditor->document()->toPlainText();
                    modelFile.close();
                } else {
                    throw InternalError("Could not write temporary model file.");
                }
                return model;
            }
        }
    }

    auto modelFiles = getProject().modelFiles();
    if (modelFiles.count() == 0) {
        // There are no model files
        return "";
    }
    if (modelFiles.count() == 1) {
        return modelFiles[0];
    }

    int selectedModel = paramDialog->getModel(modelFiles);
    if (selectedModel == -1) {
        return "";
    }
    return modelFiles[selectedModel];
}

bool MainWindow::promptSaveModified()
{
    QVector<CodeEditor*> modifiedDocs;
    for (int i=0; i<ui->tabWidget->count(); i++) {
        CodeEditor* ce = qobject_cast<CodeEditor*>(ui->tabWidget->widget(i));
        if (ce && !ce->filepath.isEmpty() && ce->document()->isModified()) {
            modifiedDocs << ce;
        }
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
                return false;
            }
        }
        for (auto ce : modifiedDocs) {
            saveFile(ce,ce->filepath);
            if (ce->document()->isModified()) {
                return false;
            }
        }
    }

    return true;
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

void MainWindow::statusTimerEvent(qint64 time)
{
    QString txt = "Running.";
    int dots = time / 5000000000;
    for (int i = 0; i < dots; i++) {
        txt += ".";
    }
    ui->statusbar->showMessage(txt);
    setElapsedTime(time);
}

void MainWindow::openJSONViewer(bool isJSONinitHandler, const QVector<MznProcess::VisOutput>& output)
{
    if (curHtmlWindow==-1) {
        QVector<VisWindowSpec> specs;
        for (auto& item : output) {
            specs << item.spec;
        }
        QFileInfo htmlWindowTitleFile(curFilePath);
        HTMLWindow* htmlWindow = new HTMLWindow(specs, this, htmlWindowTitleFile.fileName());
        curHtmlWindow = htmlWindow->getId();
        htmlWindow->init();
        connect(htmlWindow, SIGNAL(closeWindow(int)), this, SLOT(closeHTMLWindow(int)));
        htmlWindow->show();
    }
    for (int i=0; i<output.size(); i++) {
        if (htmlWindows[curHtmlWindow]) {
            if (isJSONinitHandler) {
                htmlWindows[curHtmlWindow]->initJSON(i, output[i].data);
            } else {
                htmlWindows[curHtmlWindow]->addSolution(i, output[i].data);
            }
        }
    }
}

void MainWindow::finishJSONViewer(qint64 time)
{
    if (curHtmlWindow >= 0 && htmlWindows[curHtmlWindow]) {
        htmlWindows[curHtmlWindow]->finish(time);
    }
}

void MainWindow::compile(const SolverConfiguration& sc, const QString& model, const QStringList& data, const QStringList& extraArgs, bool profile)
{
    if (!requireMiniZinc()) {
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

    auto timer = new QTimer(this);
    auto compileProcess = new MznProcess(this);
    QStringList args;
    args << "-c"
         << "-o" << fzn
         << model
         << data;
    if (profile) {
        args << "--output-paths-to-stdout"
             << "--output-detailed-timing";
    }

    connect(ui->actionStop, &QAction::triggered, compileProcess, [=] () {
        ui->actionStop->setDisabled(true);
        compileProcess->disconnect();
        compileProcess->terminate();
        addOutput("<div class='mznnotice'>Stopped.</div>");
        procFinished(0, compileProcess->timeElapsed());
        compileProcess->deleteLater();
        timer->deleteLater();
    });
    connect(compileProcess, &MznProcess::readyReadStandardError, [=]() {
       compileProcess->setReadChannel(QProcess::StandardError);
       while (compileProcess->canReadLine()) {
           auto line = QString::fromUtf8(compileProcess->readLine());
           outputStdErr(line);
       }
    });
    connect(compileProcess, QOverload<int, QProcess::ExitStatus>::of(&MznProcess::finished), [=](int exitCode, QProcess::ExitStatus exitStatus) {
        if (exitStatus == QProcess::CrashExit) {
            QMessageBox::critical(this, "MiniZinc IDE", "MiniZinc crashed unexpectedly.");
            procFinished(0, compileProcess->timeElapsed());
        } else {
            procFinished(exitCode, compileProcess->timeElapsed());
            if (exitCode == 0) {
                if (profile) {
                    auto output = QString::fromUtf8(compileProcess->readAllStandardOutput());
                    profileCompiledFzn(output);
                } else {
                    openCompiledFzn(fzn);
                }
            }
        }
        compileProcess->deleteLater();
        timer->stop();
        timer->deleteLater();
    });
    connect(compileProcess, &MznProcess::errorOccurred, [=](QProcess::ProcessError e) {
        timer->stop();
        if (e == QProcess::FailedToStart) {
            QMessageBox::critical(this, "MiniZinc IDE", "Failed to start MiniZinc. Check your path settings.");
        } else {
            QMessageBox::critical(this, "MiniZinc IDE", "Unknown error while executing MiniZinc: error code " + QString().number(e));
        }
        procFinished(0, compileProcess->timeElapsed());
        compileProcess->deleteLater();
        timer->stop();
        timer->deleteLater();
    });
    connect(timer, &QTimer::timeout, compileProcess, [=] () {
        statusTimerEvent(compileProcess->timeElapsed());
    });
    updateUiProcessRunning(true);
    compileProcess->run(sc, args, fi.absolutePath());
    timer->start(1000);
}

void MainWindow::run(const SolverConfiguration& sc, const QString& model, const QStringList& data, const QStringList& extraArgs, QTextStream* ts)
{
    if (!requireMiniZinc()) {
        return;
    }

    auto timer = new QTimer(this);
    auto solveProcess = new SolveProcess(this);
    auto writeOutput = [=](const QString& d) {
        addOutput(d, false);
        if (ts) {
            *ts << d;
        }
    };

    connect(ui->actionStop, &QAction::triggered, solveProcess, [=] () {
        ui->actionStop->setDisabled(true);
        solveProcess->disconnect();
        solveProcess->terminate();
        addOutput("<div class='mznnotice'>Stopped.</div>");
        procFinished(0, solveProcess->timeElapsed());
        solveProcess->deleteLater();
    });
    connect(solveProcess, &SolveProcess::solutionOutput, writeOutput);
    connect(solveProcess, &SolveProcess::finalStatus, writeOutput);
    connect(solveProcess, &SolveProcess::fragment, writeOutput);
    connect(solveProcess, &SolveProcess::htmlOutput, [=](const QString& html) {
        addOutput(html, true);
    });
    connect(solveProcess, &SolveProcess::jsonOutput, this, &MainWindow::openJSONViewer);
    connect(solveProcess, &SolveProcess::stdErrorOutput, this, &MainWindow::outputStdErr);
    connect(solveProcess, QOverload<int, QProcess::ExitStatus>::of(&MznProcess::finished), [=](int exitCode, QProcess::ExitStatus exitStatus) {
        if (exitStatus == QProcess::CrashExit) {
            QMessageBox::critical(this, "MiniZinc IDE", "MiniZinc crashed unexpectedly.");
            procFinished(0, solveProcess->timeElapsed());
        } else {
            procFinished(exitCode, solveProcess->timeElapsed());
        }
        solveProcess->deleteLater();
        timer->stop();
        timer->deleteLater();
    });
    connect(solveProcess, &MznProcess::errorOccurred, [=](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart) {
            QMessageBox::critical(this, "MiniZinc IDE", "Failed to start MiniZinc. Check your path settings.");
        } else {
            QMessageBox::critical(this, "MiniZinc IDE", "Unknown error while executing MiniZinc: error code " + QString().number(e));
        }
        procFinished(0, solveProcess->timeElapsed());
        solveProcess->deleteLater();
        timer->stop();
        timer->deleteLater();
    });
    connect(timer, &QTimer::timeout, solveProcess, [=] () {
        statusTimerEvent(solveProcess->timeElapsed());
    });
    curFilePath = model; // TODO: Fix the JSON visualization handling to not require this
    updateUiProcessRunning(true);
    solveProcess->solve(sc, model, data, extraArgs);
    timer->start(1000);
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
            run(*getCurrentSolverConfig(), htmlWindowModels[htmlWindowIdentifier], dataFiles);
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
    stop();
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

void MainWindow::procFinished(int exitCode, qint64 time) {
    procFinished(exitCode);
    QString elapsedTime = setElapsedTime(time);
    ui->statusbar->clearMessage();
    addOutput("<div class='mznnotice'>Finished in " + elapsedTime + "</div>");
    finishJSONViewer(time);
}

void MainWindow::procFinished(int exitCode) {
    if (exitCode != 0) {
        addOutput("<div style='color:red;'>Process finished with non-zero exit code "+QString().number(exitCode)+"</div>");
    }
    updateUiProcessRunning(false);
    ui->statusbar->clearMessage();
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
                    getProject().remove(ce->filepath);
                    getProject().add(filepath);
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
        CodeEditor* ce = qobject_cast<CodeEditor*>(ui->tabWidget->widget(i));
        if (ce && ce->filepath==oldPath) {
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
        CodeEditor* ce = qobject_cast<CodeEditor*>(ui->tabWidget->widget(i));
        if (ce && ce->filepath.endsWith(".mzn")) {
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
        CodeEditor* ce = qobject_cast<CodeEditor*>(ui->tabWidget->widget(i));
        if (ce) {
            ce->setEditorFont(font);
        }
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
          CodeEditor* ce = qobject_cast<CodeEditor*>(ui->tabWidget->widget(i));
          if (!ce) {
              continue;
          }

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
       CodeEditor* ce = qobject_cast<CodeEditor*>(ui->tabWidget->widget(ui->tabWidget->currentIndex()));
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
    if (!ce) {
        continue;
    }
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
          CodeEditor* ce = qobject_cast<CodeEditor*>(ui->tabWidget->widget(i));
          if (ce) {
              Highlighter& hl = ce->getHighlighter();
              hl.clearFixedBg();
              hl.rehighlight();
          }
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
        CodeEditor* ce = qobject_cast<CodeEditor*>(ui->tabWidget->widget(i));
        if (!ce) {
            continue;
        }
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
        return !getProject().hasProjectFile();
    }
    if (getProject().isModified()) {
        return true;
    }
    for (int i = 0; i < ui->tabWidget->count(); i++) {
        CodeEditor* ce = qobject_cast<CodeEditor*>(ui->tabWidget->widget(i));
        if (ce && (!ce->filepath.isEmpty() || ce->document()->isModified())) {
            return false;
        }
    }
    return true;
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
                    CodeEditor* ce = qobject_cast<CodeEditor*>(ui->tabWidget->widget(closeTab));
                    if (ce && ce->filepath == "")
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
    QString filepath = f;
    if (filepath.isEmpty()) {
        filepath = QFileDialog::getSaveFileName(this, "Save project", getLastPath(), "MiniZinc projects (*.mzp)");
        if (!filepath.isNull()) {
            setLastPath(QFileInfo(filepath).absolutePath() + fileDialogSuffix);
        }
    }
    if (filepath.isEmpty()) {
        return;
    }

    auto& p = getProject();
    if (p.projectFile() != filepath && IDE::instance()->projects.contains(filepath)) {
        QMessageBox::warning(this,"MiniZinc IDE","Cannot overwrite existing open project.",
                             QMessageBox::Ok);
        return;
    }

    if (p.projectFile() != filepath) {
        p.projectFile(filepath);
    }
    p.openTabsChanged(getOpenFiles(), ui->tabWidget->currentIndex());
    p.saveProject();
}

void MainWindow::loadProject(const QString& filepath)
{
    try {
        auto& p = getProject();
        auto warnings = p.loadProject(filepath, ui->config_window);
        if (ui->projectExplorerDockWidget->isHidden()) {
            on_actionShow_project_explorer_triggered();
        }
        if (!warnings.empty()) {
            QMessageBox::warning(this, "MiniZinc IDE",
                                 warnings.join("\n"),
                                 QMessageBox::Ok);
        }

        int openTab = p.openTab();
        auto openFiles = p.openFiles();
        for (int i = 0; i < openFiles.count(); i++) {
            openFile(openFiles[i], false, i == openTab);
        }
        p.setModified(false);
    } catch (Exception& e) {
        QMessageBox::warning(this, "MiniZinc IDE",
                             e.message(),
                             QMessageBox::Ok);
    }
}

void MainWindow::on_actionSave_project_triggered()
{
    saveProject(getProject().projectFile());
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
        CodeEditor* ce = qobject_cast<CodeEditor*>(ui->tabWidget->widget(i));
        if (ce && ce->document()->isModified())
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
        CodeEditor* ce = qobject_cast<CodeEditor*>(ui->tabWidget->widget(i));
        if (ce) {
            ce->showDebugInfo(profileInfoVisible);
        }
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
    for (auto problem : getProject().moocAssignment().problems) {
        files.insert(problem.model);
        files.insert(problem.data);
    }
    for (int i = 0; i < ui->tabWidget->count(); i++) {
        auto ce = qobject_cast<CodeEditor*>(ui->tabWidget->widget(i));
        if (ce && ce->document()->isModified() && files.contains(ce->filepath)) {
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

    moocSubmission = new MOOCSubmission(this, getProject().moocAssignment());
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
        CodeEditor* ce = qobject_cast<CodeEditor*>(ui->tabWidget->widget(i));
        if (ce) {
            ce->setDarkMode(darkMode);
        }
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
    disconnect(solverConfCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), ui->config_window, &ConfigWindow::setCurrentIndex);
    int oldIndex = solverConfCombo->currentIndex();
    solverConfCombo->clear();
    solverConfCombo->addItems(items);
    solverConfCombo->setCurrentIndex(oldIndex);
    connect(solverConfCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), ui->config_window, &ConfigWindow::setCurrentIndex);

    ui->menuSolver_configurations->clear();
    for (auto& item: items) {
        auto action = ui->menuSolver_configurations->addAction(item);
        action->setCheckable(true);
        action->setChecked(solverConfCombo->currentText() == item);
    }
    ui->menuSolver_configurations->addSeparator();
    ui->menuSolver_configurations->addAction(ui->actionEditSolverConfig);

    bool canSaveAll = false;
    for (auto sc : ui->config_window->solverConfigs()) {
        if (!sc->isBuiltin && sc->modified) {
            if (sc->paramFile.isEmpty()) {
                canSaveAll = false;
                break;
            } else {
                canSaveAll = true;
            }
        }
    }
    ui->actionSave_all_solver_configurations->setEnabled(canSaveAll);
}

void MainWindow::on_config_window_selectedIndexChanged(int index)
{
    solverConfCombo->setCurrentIndex(index);

    for (auto action : ui->menuSolver_configurations->actions()) {
        action->setChecked(solverConfCombo->currentText() == action->text());
    }

    auto sc = getCurrentSolverConfig();
    ui->actionSave_solver_configuration->setDisabled(!sc);

    updateProfileSearchButton();
}

void MainWindow::on_menuSolver_configurations_triggered(QAction* action)
{
    if (action == ui->actionEditSolverConfig) {
        return;
    }

    auto actions = ui->menuSolver_configurations->actions();
    ui->config_window->setCurrentIndex(actions.indexOf(action));
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

void MainWindow::on_moocChanged(const MOOCAssignment* mooc)
{
    if (mooc) {
        ui->actionSubmit_to_MOOC->setVisible(true);
        ui->actionSubmit_to_MOOC->setText("Submit to " + mooc->moocName);
        QIcon icon = mooc->moocName == "Coursera" ?
                    QIcon(":/icons/images/coursera.png") :
                    QIcon(":/icons/images/application-certificate.png");
        ui->actionSubmit_to_MOOC->setIcon(icon);
    } else {
        ui->actionSubmit_to_MOOC->setVisible(false);
    }
}

QStringList MainWindow::getOpenFiles()
{
    QStringList ret;
    for (int i=0; i<ui->tabWidget->count(); i++) {
        CodeEditor* ce = qobject_cast<CodeEditor*>(ui->tabWidget->widget(i));
        if (ce) {
            ret << ce->filepath;
        } else {
            ret << "";
        }
    }
    return ret;
}

void MainWindow::on_projectBrowser_runRequested(const QStringList& files)
{
    QString model;
    QStringList data;
    QString checker;
    auto sc = getCurrentSolverConfig();
    for (auto& f : files) {
        if (f.endsWith(".mpc")) {
            int i = ui->config_window->findConfigFile(f);
            if (i == -1) {
                sc = ui->config_window->solverConfigs()[i];
            }
        } else if (f.endsWith(".mzc.mzn") || f.endsWith(".mzc")) {
            checker = f;
        } else if (model.isEmpty() && f.endsWith(".mzn")) {
            model = f;
        } else {
            data << f;
        }
    }

    compileOrRun(CM_RUN, sc, model, data, checker);
}

void MainWindow::on_projectBrowser_openRequested(const QStringList& files)
{
    QStringList openFiles = getOpenFiles();
    for (auto& f : files) {
        if (f.endsWith(".mpc")) {
            int i = 0;
            for (auto sc : ui->config_window->solverConfigs()) {
                if (sc->paramFile == f) {
                    ui->config_window->setCurrentIndex(i);
                    break;
                }
                i++;
            }
            continue;
        }

        int index = openFiles.indexOf(f);
        if (index == -1) {
            createEditor(f, false, false, false, true);
        } else {
            ui->tabWidget->setCurrentIndex(index);
        }
    }
}

void MainWindow::on_projectBrowser_removeRequested(const QStringList& files)
{
    bool modified = false;
    QList<CodeEditor*> editors;
    for (int i = 0; i<ui->tabWidget->count(); i++) {
        CodeEditor* ce = qobject_cast<CodeEditor*>(ui->tabWidget->widget(i));
        if (ce && files.contains(QFileInfo(ce->filepath).absoluteFilePath())) {
            editors << ce;
            if (ce->document()->isModified()) {
                modified = true;
            }
        }
    }

    QVector<int> scIndexes;
    for (auto& file : files) {
        if (file.endsWith(".mpc")) {
            int i = ui->config_window->findConfigFile(file);
            scIndexes << i;
            if (ui->config_window->solverConfigs()[i]->modified) {
                modified = true;
            }
        }
    }
    std::sort(scIndexes.begin(), scIndexes.end(), std::greater<int>());

    if (modified) {
        int ret = QMessageBox::warning(this, "MiniZinc IDE",
                                       "There are modified documents.\nDo you want to discard the changes or cancel?",
                                       QMessageBox::Discard | QMessageBox::Cancel);
        if (ret == QMessageBox::Cancel) {
            return;
        }
    }

    for (auto ce : editors) {
        ce->document()->setModified(false);
        ui->tabWidget->removeTab(ui->tabWidget->indexOf(ce));
        IDE::instance()->removeEditor(ce->filepath,ce);
        delete ce;
    }

    for (auto i : scIndexes) {
        ui->config_window->removeConfig(i);
    }

    getProject().openTabsChanged(getOpenFiles(), ui->tabWidget->currentIndex());
}

void MainWindow::on_actionSave_solver_configuration_triggered()
{
    ui->config_window->saveConfig();
}

void MainWindow::on_actionSave_all_solver_configurations_triggered()
{
    auto& configs = ui->config_window->solverConfigs();
    for (int i = 0; i < configs.count(); i++) {
        if (!configs[i]->isBuiltin && configs[i]->modified) {
            ui->config_window->saveConfig(i);
        }
    }
}

void MainWindow::stop()
{
    ui->actionStop->trigger();
}

void MainWindow::on_actionShow_search_profiler_triggered()
{
    if (!conductor) {
        auto profiler_layout = new QGridLayout(this);
        ui->cpprofiler->setLayout(profiler_layout);

        cpprofiler::Options opts;
        conductor = new cpprofiler::Conductor(opts, this);
        conductor->setWindowFlags(Qt::Widget);
        connect(conductor, &cpprofiler::Conductor::showExecutionWindow, this, &MainWindow::showExecutionWindow);
        connect(conductor, &cpprofiler::Conductor::showMergeWindow, this, &MainWindow::showMergeWindow);
        profiler_layout->addWidget(conductor);

        ex_id = 0;
    }
    if (ui->cpprofiler_dockWidget->isVisible()) {
        ui->cpprofiler_dockWidget->hide();
    } else {
        ui->cpprofiler_dockWidget->show();
    }
}

void MainWindow::on_actionProfile_search_triggered()
{
    if (!ui->cpprofiler_dockWidget->isVisible()) {
        on_actionShow_search_profiler_triggered();
    }
    curHtmlWindow = -1;
    QStringList args;
    args << "--cp-profiler"
         << QString::number(ex_id++) + "," + QString::number(conductor->getListenPort());
    compileOrRun(CM_RUN, nullptr, QString(), QStringList(), QString(), args);
}

void MainWindow::showExecutionWindow(cpprofiler::ExecutionWindow& e)
{
    e.setWindowFlags(Qt::Widget);
    e.setParent(this);
    for (int i = 0; i < ui->tabWidget->count(); i++) {
        if (ui->tabWidget->widget(i) == &e) {
            ui->tabWidget->setCurrentIndex(i);
            return;
        }
    }
    auto name = QString::fromStdString(e.execution().name());
    ui->tabWidget->addTab(&e, name);
    ui->tabWidget->setCurrentIndex(ui->tabWidget->count()-1);
}

void MainWindow::showMergeWindow(cpprofiler::analysis::MergeWindow& m)
{
    m.setWindowFlags(Qt::Widget);
    m.setParent(this);
    for (int i = 0; i < ui->tabWidget->count(); i++) {
        if (ui->tabWidget->widget(i) == &m) {
            ui->tabWidget->setCurrentIndex(i);
            return;
        }
    }
    auto name = QString::fromStdString("Merge result");
    ui->tabWidget->addTab(&m, name);
    ui->tabWidget->setCurrentIndex(ui->tabWidget->count()-1);
}

void MainWindow::on_cpprofiler_dockWidget_visibilityChanged(bool visible)
{
    if (visible) {
        ui->actionShow_search_profiler->setText("Hide search profiler");
    } else {
        ui->actionShow_search_profiler->setText("Show search profiler");
    }
}

void MainWindow::updateProfileSearchButton()
{
    auto sc = getCurrentSolverConfig();
    ui->actionProfile_search->setDisabled(!curEditor || !sc || !sc->solverDefinition.stdFlags.contains("--cp-profiler"));
}
