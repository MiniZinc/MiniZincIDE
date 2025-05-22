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
#include "ideutils.h"
#include "preferencesdialog.h"

#include "cp-profiler/src/cpprofiler/execution.hh"
#include "cp-profiler/src/cpprofiler/options.hh"

#include <QtGlobal>

#define BUILD_YEAR  (__DATE__ + 7)

MainWindow::MainWindow(const QString& project) :
    ui(new Ui::MainWindow),
    curEditor(nullptr),
    code_checker(nullptr),
    tmpDir(nullptr),
    saveBeforeRunning(false),
    processRunning(false)
{
    init(project);
}

MainWindow::MainWindow(const QStringList& files) :
    ui(new Ui::MainWindow),
    curEditor(nullptr),
    code_checker(nullptr),
    tmpDir(nullptr),
    saveBeforeRunning(false),
    processRunning(false)
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
        QVariant v = QVariant::fromValue(static_cast<void*>(*it));
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
    code_checker = new CodeChecker(this);

    IDE::instance()->mainWindows.insert(this);
    ui->setupUi(this);
    ui->tabWidget->removeTab(0);
#ifndef Q_OS_MAC
    ui->menuFile->addAction(ui->actionQuit);
#endif

    setCorner(Qt::BottomRightCorner, Qt::RightDockWidgetArea);

    // initialise find widget
    ui->findFrame->hide();
    ui->findWidget->installEventFilter(this);

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

    setAcceptDrops(true);
    setAttribute(Qt::WA_DeleteOnClose, true);
    minimizeAction = new QAction("&Minimize",this);
    minimizeAction->setShortcut(Qt::CTRL | Qt::Key_M);
#ifdef Q_OS_MAC
    connect(ui->menuWindow, &QMenu::aboutToShow, this, &MainWindow::showWindowMenu);
    connect(ui->menuWindow, &QMenu::triggered, this, &MainWindow::windowMenuSelected);
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
    fakeRunAction->setShortcut(Qt::CTRL | Qt::Key_R);
    fakeRunAction->setEnabled(true);
    this->addAction(fakeRunAction);

    fakeCompileAction = new QAction(this);
    fakeCompileAction->setShortcut(Qt::CTRL | Qt::Key_B);
    fakeCompileAction->setEnabled(true);
    this->addAction(fakeCompileAction);

    fakeStopAction = new QAction(this);
    fakeStopAction->setShortcut(Qt::CTRL | Qt::Key_E);
    fakeStopAction->setEnabled(true);
    this->addAction(fakeStopAction);

    updateRecentProjects("");
    updateRecentFiles("");
    connect(ui->menuRecent_Files, &QMenu::triggered, this, &MainWindow::recentFileMenuAction);
    connect(ui->menuRecent_Projects, &QMenu::triggered, this, &MainWindow::recentProjectMenuAction);

    connect(ui->tabWidget, &QTabWidget::tabCloseRequested, this, &MainWindow::tabCloseRequest);
    connect(ui->tabWidget, &QTabWidget::currentChanged, this, &MainWindow::tabChange);

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

    connect(ui->outputWidget, &OutputWidget::anchorClicked, this, &MainWindow::anchorClicked);

    QSettings settings;
    settings.beginGroup("MainWindow");
    editorFont = IDEUtils::fontFromString(settings.value("editorFont").toString());
    auto zoom = settings.value("zoom", 100).toInt();
    editorFont.setPointSize(editorFont.pointSize() * zoom / 100);
    initTheme();
    ui->outputWidget->setBrowserFont(editorFont);
    resize(settings.value("size", QSize(800, 600)).toSize());
    move(settings.value("pos", QPoint(100, 100)).toPoint());

    auto frameGeom = frameGeometry();
    bool positionValid = false;
    for (auto* screen: QGuiApplication::screens()) {
        auto geom = screen->availableGeometry();
        if (geom.intersects(frameGeom)) {
            positionValid = true;
            break;
        }
    }
    if (!positionValid) {
        // Window is outside of view
        resize(800, 600);
        move(100, 100);
    }

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

    settings.beginGroup("ide");
    indentSize = settings.value("indentSize", 2).toInt();
    useTabs = settings.value("indentTabs", false).toBool();
    settings.endGroup();

    IDE::instance()->setEditorFont(editorFont);

    settings.beginGroup("minizinc");
    QString mznDistribPath = settings.value("mznpath","").toString();
    settings.endGroup();
    auto& driver = MznDriver::get();
    if (!driver.isValid()) {
        try {
            driver.setLocation(mznDistribPath);
        } catch (Exception& e) {
            int ret = QMessageBox::warning(this, "MiniZinc IDE",
                                           e.message() + "\nDo you want to open the settings dialog?",
                                           QMessageBox::Ok | QMessageBox::Cancel);
            if (ret == QMessageBox::Ok)
                on_actionManage_solvers_triggered();
        }
    }
    ui->config_window->init();

    connect(QApplication::clipboard(), &QClipboard::dataChanged, this, &MainWindow::onClipboardChanged);

    ui->projectExplorerDockWidget->hide();
    ui->configWindow_dockWidget->hide();

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
        ui->actionProfile_search->setEnabled(false);
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
            isMzn = (isPlayground && !curEditor->filename.endsWith(".fzn")) || QFileInfo(curEditor->filepath).suffix()=="mzn";
            isFzn = QFileInfo(curEditor->filepath).suffix()=="fzn" || curEditor->filename.endsWith(".fzn");
            isData = QFileInfo(curEditor->filepath).suffix()=="dzn" || QFileInfo(curEditor->filepath).suffix()=="json";
        }
        fakeRunAction->setEnabled(! (isMzn || isFzn || isData));
        ui->actionRun->setEnabled(isMzn || isFzn || isData);
        ui->actionProfile_compilation->setEnabled(isMzn || isData);
        ui->actionProfile_search->setEnabled(isMzn || isFzn || isData);
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
    delete code_checker;
    delete server;
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
    if (isNewFile && path == "Playground") {
        absPath = path;
        fileContents = "% Use this editor as a MiniZinc scratch book\n";
        openAsModified = false;
    } else if (isNewFile && path.startsWith(".")) {
        absPath = QString("Untitled")+QString().setNum(newFileCounter++)+path;
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
        if (isNewFile) {
            absPath = QFileInfo(path).fileName();
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
        auto& theme = IDE::instance()->themeManager->current();
        CodeEditor* ce = new CodeEditor(doc,absPath,isNewFile,large,editorFont,indentSize,useTabs,theme,darkMode,ui->tabWidget,this);
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
            auto* history = getProject().history();
            if (history != nullptr) {
                history->updateFileContents(absPath, fileContents);
            }
        } else if (doc) {
            if (!absPath.endsWith(".fzn")) {
                getProject().add(absPath);
                auto* history = getProject().history();
                if (history != nullptr) {
                    history->updateFileContents(absPath, doc->toPlainText());
                }
            }
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
    QStringList fileNames;
    if (path.isEmpty()) {
        fileNames = QFileDialog::getOpenFileNames(this, tr("Open File"), getLastPath(), "MiniZinc Files (*.mzn *.dzn *.fzn *.json *.mzp *.mzc *.mpc);;Other (*)");
        if (!fileNames.isEmpty()) {
            setLastPath(QFileInfo(fileNames.last()).absolutePath() + fileDialogSuffix);
        }
    } else {
        fileNames << path;
    }

    for (auto& fileName : fileNames) {
        if (fileName.endsWith(".mzp")) {
            openProject(fileName);
        } else if (fileName.endsWith(".mpc")) {
            QString absPath = QFileInfo(fileName).canonicalFilePath();
            if (ui->config_window->addConfig(absPath)) {
                getProject().add(absPath);
                ui->configWindow_dockWidget->setVisible(true);
            }
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

    // At this point we're definitely closing
    emit terminating();

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
    if (getProject().hasProjectFile()) {
        IDE::instance()->projects.remove(getProject().projectFile());
    }

    IDE::instance()->mainWindows.remove(this);

    QSettings settings;
    settings.beginGroup("MainWindow");
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

void MainWindow::tabChange(int tab) {
    if (curEditor) {
        disconnect(ui->actionCopy, &QAction::triggered, curEditor, &CodeEditor::copy);
        disconnect(ui->actionPaste, &QAction::triggered, curEditor, &CodeEditor::paste);
        disconnect(ui->actionCut, &QAction::triggered, curEditor, &CodeEditor::cut);
        disconnect(ui->actionSelect_All, &QAction::triggered, curEditor, &CodeEditor::selectAll);
        disconnect(ui->actionUndo, &QAction::triggered, curEditor, &CodeEditor::undo);
        disconnect(ui->actionRedo, &QAction::triggered, curEditor, &CodeEditor::redo);
        disconnect(curEditor, &CodeEditor::copyAvailable, ui->actionCopy, &QAction::setEnabled);
        disconnect(curEditor, &CodeEditor::copyAvailable, ui->actionCut, &QAction::setEnabled);
        disconnect(curEditor->document(), &QTextDocument::modificationChanged,
                   this, &MainWindow::setWindowModified);
        disconnect(curEditor->document(), &QTextDocument::undoAvailable,
                   ui->actionUndo, &QAction::setEnabled);
        disconnect(curEditor->document(), &QTextDocument::redoAvailable,
                   ui->actionRedo, &QAction::setEnabled);
        disconnect(curEditor, &CodeEditor::cursorPositionChanged, this, &MainWindow::editor_cursor_position_changed);
        disconnect(code_checker, &CodeChecker::finished, curEditor, &CodeEditor::checkFile);
        disconnect(curEditor, &CodeEditor::changedDebounced, this, &MainWindow::check_code);
    }
    curEditor = tab == -1 ? nullptr : qobject_cast<CodeEditor*>(ui->tabWidget->widget(tab));
    if (!curEditor) {
        setEditorMenuItemsEnabled(false);
        ui->findFrame->hide();
    } else {
        setEditorMenuItemsEnabled(true);
        connect(ui->actionCopy, &QAction::triggered, curEditor, &CodeEditor::copy);
        connect(ui->actionPaste, &QAction::triggered, curEditor, &CodeEditor::paste);
        connect(ui->actionCut, &QAction::triggered, curEditor, &CodeEditor::cut);
        connect(ui->actionSelect_All, &QAction::triggered, curEditor, &CodeEditor::selectAll);
        connect(ui->actionUndo, &QAction::triggered, curEditor, &CodeEditor::undo);
        connect(ui->actionRedo, &QAction::triggered, curEditor, &CodeEditor::redo);
        connect(curEditor, &CodeEditor::copyAvailable, ui->actionCopy, &QAction::setEnabled);
        connect(curEditor, &CodeEditor::copyAvailable, ui->actionCut, &QAction::setEnabled);
        connect(curEditor->document(), &QTextDocument::modificationChanged,
                this, &MainWindow::setWindowModified);
        connect(curEditor->document(), &QTextDocument::undoAvailable,
                ui->actionUndo, &QAction::setEnabled);
        connect(curEditor->document(), &QTextDocument::redoAvailable,
                ui->actionRedo, &QAction::setEnabled);
        connect(curEditor, &CodeEditor::cursorPositionChanged, this, &MainWindow::editor_cursor_position_changed);
        connect(code_checker, &CodeChecker::finished, curEditor, &CodeEditor::checkFile);
        connect(curEditor, &CodeEditor::changedDebounced, this, &MainWindow::check_code);
        setWindowModified(curEditor->document()->isModified());
        QString p;
        p += " ";
        p += QChar(0x2014);
        p += " ";
        if (getProject().hasProjectFile()) {
            QFileInfo fi(getProject().projectFile());
            p += "Project: " + fi.baseName();
        } else {
            p += "Untitled Project";
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
    if (html) {
        ui->outputWidget->addHtml(s);
    } else {
        ui->outputWidget->addText(s);
    }
}

void MainWindow::on_actionRun_triggered()
{
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

    if (curEditor != nullptr && curEditor->filepath.endsWith(".mzc.mzn") && cm == CM_COMPILE) {
        // Compile the current checker
        compileSolutionChecker(curEditor->filepath);
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
    QStringList inlineData;

    // If we didn't specify any data but the current file is a data file, use it
    if (dataFiles.isEmpty() && curEditor && (curEditor->filepath.endsWith(".dzn") || curEditor->filepath.endsWith(".json"))) {
        dataFiles << curEditor->filepath;
    }

    // Prompt for data if necessary
    if (!getModelParameters(*solverConfig, modelFile, dataFiles, extraArguments, inlineData)) {
        return;
    }

    // If we didn't specify a checker but the current file is one, use it
    if (checkerFile.isEmpty() && curEditor && (curEditor->filepath.endsWith(".mzc") || curEditor->filepath.endsWith(".mzc.mzn"))) {
        checkerFile = curEditor->filepath;
    }

    // Use checker that matches model file if it exists and checking is on
    QSettings settings;
    settings.beginGroup("ide");

    if (settings.value("clearOutput", false).toBool()) {
        on_actionClear_output_triggered();
    }
    on_actionSplit_triggered();

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

    if (solverConfig->solverDefinition.stdFlags.contains("--output-html")) {
        extraArguments << "--output-html";
    }

    // Compile/run
    switch (cm) {
    case CM_RUN:
        run(*solverConfig, modelFile, dataFiles, extraArguments, inlineData);
        break;
    case CM_COMPILE:
        compile(*solverConfig, modelFile, dataFiles, extraArguments, inlineData);
        break;
    case CM_PROFILE:
        compile(*solverConfig, modelFile, dataFiles, extraArguments, inlineData, true);
        break;
    }
}

bool MainWindow::getModelParameters(const SolverConfiguration& sc, const QString& model, QStringList& data, const QStringList& extraArgs, QStringList& inlineData)
{
    if (!requireMiniZinc()) {
        return false;
    }

    // Get model interface to obtain parameters
    QStringList args;
    args << "-c" << "--model-interface-only" << data << model << extraArgs;

    MznProcess p;
    // Passing all flags can break --model-inteface-only, so just pass the necessary ones
    SolverConfiguration checkConfig(sc.solverDefinition);
    checkConfig.additionalData = sc.additionalData;
    checkConfig.extraOptions = sc.extraOptions;

    try {
        auto result = p.run(checkConfig, args, QFileInfo(model).absolutePath());

        QStringList additionalDataFiles = data;
        if (result.exitCode == 0) {
            auto jdoc = QJsonDocument::fromJson(result.stdOut.toUtf8());
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
                                inlineData << undefinedArgs[i] + " = " + params[i];
                            }
                        }
                    }
                }
            } else {
                ui->outputWidget->addText("Error when checking model parameters:\n", ui->outputWidget->errorCharFormat());
                ui->outputWidget->addText(result.stdErr);
                QMessageBox::critical(this, "Internal error", "Could not determine model parameters");
                return false;
            }
        }
        for (const auto& d : additionalDataFiles) {
            if (!data.contains(d)) {
                // Ensure we don't add the same data file twice
                data << d;
            }
        }
        return true;
    } catch (ProcessError& e) {
         QMessageBox::critical(this, "MiniZinc IDE", e.message());
         return false;
    }
}

QString MainWindow::currentModelFile()
{
    if (curEditor) {
        if (curEditor->filepath.endsWith(".fzn") ||
                (curEditor->filepath.endsWith(".mzn") && !curEditor->filepath.endsWith(".mzc.mzn"))) {
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
                QString model = modelTmpDir->path() + "/untitled_model." + (curEditor->filename.endsWith(".fzn") ? "fzn" : "mzn");
                QFile modelFile(model);
                if (modelFile.open(QIODevice::ReadWrite)) {
                    QTextStream ts(&modelFile);
#if QT_VERSION < 0x060000
                    ts.setCodec("UTF-8");
#endif
                    ts << curEditor->document()->toPlainText();
                    modelFile.close();
                } else {
                    throw InternalError("Could not write temporary model file.");
                }
                curEditor->playgroundTempFile = model;
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
    auto elapsed = IDEUtils::formatTime(elapsed_t);
    QString timeLimit;
    auto sc = getCurrentSolverConfig();
    if (sc && sc->timeLimit > 0) {
        timeLimit += " / ";
        timeLimit += IDEUtils::formatTime(sc->timeLimit);
    }
    statusLabel->setText(elapsed + timeLimit);
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

void MainWindow::compileSolutionChecker(const QString& checker)
{
    if (!requireMiniZinc()) {
        return;
    }

    QFileInfo fi(checker);

    QSettings settings;
    settings.beginGroup("ide");
    bool printCommand = settings.value("printCommand", false).toBool();
    settings.endGroup();

    auto proc = new MznProcess(this);
    QStringList args;
    args << "--compile-solution-checker"
         << checker;
    connect(this, &MainWindow::terminating, proc, [=] () {
        proc->terminate();
        proc->deleteLater();
    });
    connect(ui->actionStop, &QAction::triggered, proc, [=] () {
        ui->actionStop->setDisabled(true);
        proc->stop();
        ui->outputWidget->addText("Stopped.");
    });
    connect(proc, &MznProcess::success, [=] (bool cancelled) {
        if (!cancelled) {
            openCompiledFzn(checker.left(checker.size() - 4));
        }
        procFinished(0, proc->elapsedTime());
        ui->outputWidget->endExecution(0, proc->elapsedTime());
    });
    connect(proc, &MznProcess::errorOutput, this, &MainWindow::on_minizincError);
    connect(proc, &MznProcess::warningOutput, this, &MainWindow::on_minizincError);
    connect(proc, &MznProcess::outputStdError, this, [=] (const QString& d) {
        QTextCharFormat f;
        f.setForeground(IDE::instance()->themeManager->current().commentColor.get(darkMode));
        ui->outputWidget->addText(d, f, "Standard Error");
    });
    connect(proc, &MznProcess::finished, [=] () {
        proc->deleteLater();
    });
    connect(proc, &MznProcess::failure, [=](int exitCode, MznProcess::FailureType e) {
        if (e == MznProcess::FailedToStart) {
            QMessageBox::critical(this, "MiniZinc IDE", "Failed to start MiniZinc. Check your path settings.");
            exitCode = 0;
        } else if (e != MznProcess::NonZeroExit) {
            QMetaEnum metaEnum = QMetaEnum::fromType<MznProcess::FailureType>();
            QMessageBox::critical(this, "MiniZinc IDE", "Unknown error while executing MiniZinc: " + QString(metaEnum.valueToKey(e)));
        }
        ui->outputWidget->endExecution(0, proc->elapsedTime());
        procFinished(exitCode, proc->elapsedTime());
    });
    connect(proc, &MznProcess::timeUpdated, this, &MainWindow::statusTimerEvent);
    updateUiProcessRunning(true);
    ui->outputWidget->startExecution(QString("Compiling %1").arg(fi.fileName()));
    proc->start(args, fi.canonicalPath());

    if (printCommand) {
        auto cmdMessage = QString("Command: %1\n").arg(proc->command());
        ui->outputWidget->addText(cmdMessage, ui->outputWidget->infoCharFormat(), "Commands");
    }
}

void MainWindow::compile(const SolverConfiguration& sc, const QString& model, const QStringList& data, const QStringList& extraArgs, const QStringList& inlineData, bool profile)
{
    if (!requireMiniZinc()) {
        return;
    }

    QFileInfo fi(model);

    QSettings settings;
    settings.beginGroup("ide");
    bool printCommand = settings.value("printCommand", false).toBool();
    settings.endGroup();

    QTemporaryDir* fznTmpDir = new QTemporaryDir;
    if (!fznTmpDir->isValid()) {
        QMessageBox::critical(this, "MiniZinc IDE", "Could not create temporary directory for compilation.");
        return;
    }
    cleanupTmpDirs.append(fznTmpDir);
    QString fzn = fznTmpDir->path() + "/" + fi.baseName() + ".fzn";

    auto proc = new MznProcess(this);
    QStringList args;
    args << "-c"
         << "-o" << fzn
         << model
         << data
         << extraArgs;
    for (auto dzn : inlineData) {
        args << "-D" << dzn;
    }
    if (profile) {
        args << "--output-paths-to-stdout"
             << "--output-detailed-timing";
    }
    connect(this, &MainWindow::terminating, proc, [=] () {
        proc->terminate();
        proc->deleteLater();
    });
    connect(ui->actionStop, &QAction::triggered, proc, [=] () {
        ui->actionStop->setDisabled(true);
        proc->stop();
        ui->outputWidget->addText("Stopped.");
    });

    if (profile) {
        auto* timing = new QVector<TimingEntry>;
        auto* paths = new QVector<PathEntry>;

        connect(proc, &MznProcess::profilingOutput, [=] (const QVector<TimingEntry>& t) {
            *timing << t;
        });
        connect(proc, &MznProcess::pathsOutput, [=] (const QVector<PathEntry>& p) {
            *paths << p;
        });

        connect(proc, &MznProcess::success, [=] (bool cancelled) {
            profileCompiledFzn(*timing, *paths);
            procFinished(0, proc->elapsedTime());
        });
        connect(proc, &MznProcess::finished, [=] () {
            delete timing;
            delete paths;
        });
    } else {
        connect(proc, &MznProcess::success, [=] (bool cancelled) {
            if (!cancelled) {
                openCompiledFzn(fzn);
            }
            procFinished(0, proc->elapsedTime());
            ui->outputWidget->endExecution(0, proc->elapsedTime());
        });
    }
    connect(proc, &MznProcess::statisticsOutput, ui->outputWidget, &OutputWidget::addStatistics);
    connect(proc, &MznProcess::errorOutput, this, &MainWindow::on_minizincError);
    connect(proc, &MznProcess::warningOutput, this, &MainWindow::on_minizincError);
    connect(proc, &MznProcess::progressOutput, this, &MainWindow::on_progressOutput);
    connect(proc, &MznProcess::outputStdError, this, [=] (const QString& d) {
        QTextCharFormat f;
        f.setForeground(IDE::instance()->themeManager->current().commentColor.get(darkMode));
        ui->outputWidget->addText(d, f, "Standard Error");
    });
    connect(proc, &MznProcess::finished, [=] () {
        proc->deleteLater();
    });
    connect(proc, &MznProcess::failure, [=](int exitCode, MznProcess::FailureType e) {
        if (e == MznProcess::FailedToStart) {
            QMessageBox::critical(this, "MiniZinc IDE", "Failed to start MiniZinc. Check your path settings.");
            exitCode = 0;
        } else if (e != MznProcess::NonZeroExit) {
            QMetaEnum metaEnum = QMetaEnum::fromType<MznProcess::FailureType>();
            QMessageBox::critical(this, "MiniZinc IDE", "Unknown error while executing MiniZinc: " + QString(metaEnum.valueToKey(e)));
        }
        ui->outputWidget->endExecution(0, proc->elapsedTime());
        procFinished(exitCode, proc->elapsedTime());
    });
    connect(proc, &MznProcess::timeUpdated, this, &MainWindow::statusTimerEvent);
    SolverConfiguration compileSc(sc);
    compileSc.outputTiming = false; // Remove solns2out options
    updateUiProcessRunning(true);
    QStringList files({QFileInfo(model).fileName()});
    for (auto& d : data) {
        files << QFileInfo(d).fileName();
    }
    auto label = files.join(", ").prepend("Compiling ");
    if (!inlineData.isEmpty()) {
        label.append(" with ");
        label.append(inlineData.join(", "));
    }
    ui->outputWidget->startExecution(label);
    proc->start(compileSc, args, fi.canonicalPath());

    if (printCommand) {
        auto cmdMessage = QString("Command: %1\n").arg(proc->command());
        ui->outputWidget->addText(cmdMessage, ui->outputWidget->infoCharFormat(), "Commands");
        auto confMessage = QString("Configuration:\n%1").arg(QString::fromUtf8(sc.toJSON()));
        ui->outputWidget->addText(confMessage, ui->outputWidget->infoCharFormat(), "Commands");
    }
}

void MainWindow::run(const SolverConfiguration& sc, const QString& model, const QStringList& data, const QStringList& extraArgs, const QStringList& inlineData, QTextStream* ts)
{
    if (!requireMiniZinc()) {
        return;
    }

    QFileInfo fi(model);
    auto workingDir = fi.canonicalPath();

    QSettings settings;
    settings.beginGroup("ide");
    int compressSolutions = settings.value("compressSolutions", 100).toInt();
    bool printCommand = settings.value("printCommand", false).toBool();
    settings.endGroup();

    auto* proc = new MznProcess(this);

    connect(this, &MainWindow::terminating, proc, [=] () {
        proc->terminate();
        proc->deleteLater();
    });

    QStringList args;
    args << model
         << data;
    bool compiledChecker = false;
    QStringList files;
    for (auto& arg : args) {
        QFileInfo fi(arg);
        files << fi.fileName();
        if (fi.suffix() == "mzc") {
            compiledChecker = true;
        }
    }
    args << extraArgs;
    for (auto& dzn : inlineData) {
        args << "-D" << dzn;
    }

    QString label = "Running ";
    label.append(files.join(", "));
    if (!inlineData.isEmpty()) {
        label.append(" with ").append(inlineData.join(", "));
    }

    if (sc.solverDefinition.isGUIApplication) {
        // Detach GUI app
        auto* failureCtx = new QObject(this);
        ui->outputWidget->startExecution(label + " (detached)");
        ui->outputWidget->addText("Process will continue running detached from the IDE.\n", ui->outputWidget->commentCharFormat());
        connect(proc, &MznProcess::started, this, [=] () {
            ui->outputWidget->endExecution(0, proc->elapsedTime());
            procFinished(0);
            delete failureCtx; // Disconnect the failure handler
        });
        connect(proc, &MznProcess::failure, failureCtx, [=](int exitCode, MznProcess::FailureType e) {
            if (e == MznProcess::FailedToStart) {
                QMessageBox::critical(this, "MiniZinc IDE", "Failed to start MiniZinc. Check your path settings.");
                exitCode = 0;
            } else if (e != MznProcess::NonZeroExit) {
                QMetaEnum metaEnum = QMetaEnum::fromType<MznProcess::FailureType>();
                QMessageBox::critical(this, "MiniZinc IDE", "Unknown error while executing MiniZinc: " + QString(metaEnum.valueToKey(e)));
            }
            ui->outputWidget->endExecution(exitCode, proc->elapsedTime());
            procFinished(exitCode, proc->elapsedTime());
        });
        connect(proc, &MznProcess::finished, proc, &QObject::deleteLater);
        connect(proc, &MznProcess::finished, failureCtx, &QObject::deleteLater);

        proc->start(sc, args, workingDir, ts == nullptr);
        return;
    }

    connect(ui->actionStop, &QAction::triggered, proc, [=] () {
        ui->actionStop->setDisabled(true);
        proc->stop();
        ui->outputWidget->addText("Stopped.\n", ui->outputWidget->noticeCharFormat());
    });
    if (ts) {
        // Write to a stream for the purposes of submission
        // (also disables JSON streaming output)
        connect(proc, &MznProcess::outputStdOut, [=](const QString& d) { *ts << d; });
    }

    connect(proc, &MznProcess::statisticsOutput, ui->outputWidget, &OutputWidget::addStatistics);
    connect(proc, &MznProcess::solutionOutput, ui->outputWidget, &OutputWidget::addSolution);
    connect(proc, &MznProcess::checkerOutput, ui->outputWidget, &OutputWidget::addCheckerOutput);
    connect(proc, &MznProcess::errorOutput, this, &MainWindow::on_minizincError);
    connect(proc, &MznProcess::warningOutput, this, [=] (const QJsonObject& error, bool fromChecker) {
        if (!fromChecker || !compiledChecker) {
            // Suppress warnings from compiled checkers
            on_minizincError(error);
        }
    });
    connect(proc, &MznProcess::finalStatus, ui->outputWidget, &OutputWidget::addStatus);
    connect(proc, &MznProcess::unknownOutput, [=](const QString& d) { ui->outputWidget->addText(d); });
    connect(proc, &MznProcess::commentOutput, this, [=] (const QString& d) {
        ui->outputWidget->addText(d, ui->outputWidget->commentCharFormat(), "Comments");
    });
    connect(proc, &MznProcess::progressOutput, this, &MainWindow::on_progressOutput);
    connect(proc, &MznProcess::traceOutput, this, [=] (const QString& section, const QVariant& message) {
        if (section == "trace_exp") {
            TextLayoutLock lock(ui->outputWidget);
            auto obj = message.toJsonObject();
            auto msg = obj["message"].toString();
            QRegularExpression val("\\(≡.*?\\)");
            QRegularExpressionMatchIterator val_i = val.globalMatch(msg);
            int pos = 0;
            auto loc = obj["location"].toObject();
            auto link = locationToLink(loc["filename"].toString(),
                    loc["firstLine"].toInt(),
                    loc["firstColumn"].toInt(),
                    loc["lastLine"].toInt(),
                    loc["lastColumn"].toInt(),
                    IDE::instance()->themeManager->current().warningColor.get(darkMode)
                    );
            auto prevLink = ui->outputWidget->lastTraceLoc(link);
            if (prevLink != link) {
                ui->outputWidget->addHtml(link, "trace");
                ui->outputWidget->addText(":\n", "trace");
            }

            ui->outputWidget->addText("  ", ui->outputWidget->infoCharFormat(), "trace");
            while (val_i.hasNext()) {
                auto match = val_i.next();
                if (match.capturedStart() > 0) {
                    ui->outputWidget->addText(msg.mid(pos, match.capturedStart() - pos),
                                              ui->outputWidget->infoCharFormat(), "trace");
                }
                ui->outputWidget->addText(match.captured(0), ui->outputWidget->commentCharFormat(), "trace");
                pos = match.capturedEnd();
            }
            if (pos < msg.size()) {
                ui->outputWidget->addText(msg.mid(pos, msg.size() - pos),
                                          ui->outputWidget->infoCharFormat(), "trace");
            }
            ui->outputWidget->addText("\n", ui->outputWidget->infoCharFormat(), "trace");
        } else {
            auto text = message.toString();
            if (!text.isEmpty()) {
                ui->outputWidget->addTextToSection(section, text, ui->outputWidget->commentCharFormat());
            }
            if (vis_connector == nullptr && section.startsWith("mzn_vis_")) {
                auto obj = message.toJsonObject();
                startVisualisation(model, data, section, obj["url"].toString(), obj["userData"], proc);
            }
        }
    });
    connect(proc, &MznProcess::outputStdError, this, [=] (const QString& d) {
        ui->outputWidget->addText(d, ui->outputWidget->commentCharFormat(), "Standard Error");
    });
    connect(proc, &MznProcess::success, [=]() {
        ui->outputWidget->endExecution(0, proc->elapsedTime());
        procFinished(0, proc->elapsedTime());
    });
    connect(proc, &MznProcess::finished, proc, &QObject::deleteLater);
    connect(proc, &MznProcess::failure, [=](int exitCode, MznProcess::FailureType e) {
        if (e == MznProcess::FailedToStart) {
            QMessageBox::critical(this, "MiniZinc IDE", "Failed to start MiniZinc. Check your path settings.");
            exitCode = 0;
        } else if (e != MznProcess::NonZeroExit) {
            QMetaEnum metaEnum = QMetaEnum::fromType<MznProcess::FailureType>();
            QMessageBox::critical(this, "MiniZinc IDE", "Unknown error while executing MiniZinc: " + QString(metaEnum.valueToKey(e)));
        }
        ui->outputWidget->endExecution(exitCode, proc->elapsedTime());
        procFinished(exitCode, proc->elapsedTime());
    });
    connect(proc, &MznProcess::timeUpdated, this, &MainWindow::statusTimerEvent);

    updateUiProcessRunning(true);

    vis_connector = nullptr;
    ui->outputWidget->setSolutionLimit(compressSolutions);
    ui->outputWidget->startExecution(label);

    proc->start(sc, args, workingDir, ts == nullptr);

    if (printCommand) {
        auto cmdMessage = QString("Command: %1\n").arg(proc->command());
        ui->outputWidget->addText(cmdMessage, ui->outputWidget->infoCharFormat(), "Commands");
        auto confMessage = QString("Configuration:\n%1").arg(QString::fromUtf8(sc.toJSON()));
        ui->outputWidget->addText(confMessage, ui->outputWidget->infoCharFormat(), "Commands");
    }
}

QString MainWindow::currentSolverConfigName(void) {
    SolverConfiguration* sc = getCurrentSolverConfig();
    return sc ? sc->name() : "None";
}

void MainWindow::procFinished(int exitCode, qint64 time) {
    procFinished(exitCode);
    QString elapsedTime = setElapsedTime(time);
    ui->statusbar->clearMessage();
}

void MainWindow::procFinished(int exitCode) {
    updateUiProcessRunning(false);
    ui->statusbar->clearMessage();
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
#if QT_VERSION < 0x060000
                out.setCodec(QTextCodec::codecForName("UTF-8"));
#endif
                auto contents = ce->document()->toPlainText();
                out << contents;
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
                auto* history = getProject().history();
                if (history != nullptr) {
                    history->updateFileContents(filepath, contents);
                }
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
    createEditor(fzn, !fzn.endsWith(".mzc"), !fzn.endsWith(".mzc"), false);
}

void MainWindow::profileCompiledFzn(const QVector<TimingEntry>& timing, const QVector<PathEntry>& paths)
{
    typedef QMap<QString,QVector<BracketData*>> CoverageMap;
    CoverageMap ce_coverage;

    for (int i=0; i<ui->tabWidget->count(); i++) {
        CodeEditor* ce = qobject_cast<CodeEditor*>(ui->tabWidget->widget(i));
        if (ce == nullptr) {
            continue;
        }
        auto path = ce->filepath.isEmpty() ? ce->playgroundTempFile : ce->filepath;
        if (!path.endsWith(".mzn")) {
            continue;
        }
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
        ce_coverage.insert(path,coverage);
    }

    int totalCons=1;
    int totalVars=1;
    int totalTime=1;
    for (auto& it : paths) {
        int min_line_covered = -1;
        CoverageMap::iterator fileMatch;
        for (auto& segment : it.path().segments()) {
            auto tryMatch = ce_coverage.find(segment.filename);
            if (tryMatch != ce_coverage.end()) {
                fileMatch = tryMatch;
                min_line_covered = segment.firstLine;
            }
        }
        if (min_line_covered > 0 && min_line_covered <= fileMatch.value().size()) {
            if (it.constraintIndex() != -1) {
                fileMatch.value()[min_line_covered-1]->d.con++;
                totalCons++;
            } else {
                fileMatch.value()[min_line_covered-1]->d.var++;
                totalVars++;
            }
        }
    }
    for (auto& it : timing) {
        CoverageMap::iterator fileMatch = ce_coverage.find(it.filename());
        if (fileMatch != ce_coverage.end()) {
            int line_no = it.line();
            if (line_no > 0 && line_no <= fileMatch.value().size()) {
                fileMatch.value()[line_no-1]->d.ms = it.time();
                totalTime += fileMatch.value()[line_no-1]->d.ms;
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
    ui->outputWidget->clear();
    if (server != nullptr) {
        server->clear();
    }
}

void MainWindow::setEditorFont(QFont font)
{
    editorFont = font;
    ui->outputWidget->setBrowserFont(font);
    for (int i=0; i<ui->tabWidget->count(); i++) {
        CodeEditor* ce = qobject_cast<CodeEditor*>(ui->tabWidget->widget(i));
        if (ce) {
            ce->setEditorFont(font);
        }
    }
}

void MainWindow::setEditorIndent(int indentSize, bool useTabs)
{
    for (int i = 0; i < ui->tabWidget->count(); i++) {
        auto* ce = qobject_cast<CodeEditor*>(ui->tabWidget->widget(i));
        if (ce) {
            ce->setIndentSize(indentSize);
            ce->setIndentTab(useTabs);
        }
    }
}

void MainWindow::setEditorWordWrap(QTextOption::WrapMode mode)
{

    for (int i = 0; i < ui->tabWidget->count(); i++) {
        auto* ce = qobject_cast<CodeEditor*>(ui->tabWidget->widget(i));
        if (ce) {
            ce->setWordWrapMode(mode);
        }
    }
}

void MainWindow::on_actionBigger_font_triggered()
{
    QSettings settings;
    settings.beginGroup("MainWindow");
    auto zoom = std::min(settings.value("zoom", 100).toInt() + 10, 10000);
    settings.setValue("zoom", zoom);
    auto font = IDEUtils::fontFromString(settings.value("editorFont").toString());
    settings.endGroup();
    font.setPointSize(font.pointSize() * zoom / 100);
    IDE::instance()->setEditorFont(font);
}

void MainWindow::on_actionSmaller_font_triggered()
{
    QSettings settings;
    settings.beginGroup("MainWindow");
    auto zoom = std::max(10, settings.value("zoom", 100).toInt() - 10);
    settings.setValue("zoom", zoom);
    auto font = IDEUtils::fontFromString(settings.value("editorFont").toString());
    settings.endGroup();
    font.setPointSize(font.pointSize() * zoom / 100);
    IDE::instance()->setEditorFont(font);
}

void MainWindow::on_actionDefault_font_size_triggered()
{
    QSettings settings;
    settings.beginGroup("MainWindow");
    settings.setValue("zoom", 100);
    auto font = IDEUtils::fontFromString(settings.value("editorFont").toString());
    settings.endGroup();
    font.setPointSize(font.pointSize());
    IDE::instance()->setEditorFont(font);
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
            QRegularExpression re(toFind, ignoreCase ? QRegularExpression::CaseInsensitiveOption : QRegularExpression::NoPatternOption);
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

void MainWindow::anchorClicked(const QUrl & anUrl)
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

    if (conductor != nullptr && url.scheme() == "cpprofiler") {
        bool ok;
        auto ex_id = url.query().toInt(&ok);
        if (ok) {
            auto* ex = conductor->getExecution(ex_id);
            if (ex != nullptr) {
                showExecutionWindow(conductor->getExecutionWindow(ex));
            }
        }
        return;
    }

    if(url.scheme() == "http" || url.scheme() == "https") {
      QDesktopServices::openUrl(url);
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
        QFileInfo ceinfo(ce->filepath.isEmpty() ? ce->playgroundTempFile : ce->filepath);
        if (ceinfo.canonicalFilePath() == urlinfo.canonicalFilePath()) {
            QRegularExpression re_line("line=([0-9]+)");
            auto re_line_match = re_line.match(query);
            if (re_line_match.hasMatch()) {
                bool ok;
                int line = re_line_match.captured(1).toInt(&ok);
                if (ok) {
                    int col = 1;
                    QRegularExpression re_col("column=([0-9]+)");
                    auto re_col_match = re_col.match(query);
                    if (re_col_match.hasMatch()) {
                        bool ok;
                        col = re_col_match.captured(1).toInt(&ok);
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

    ui->config_window->stashModifiedConfigs();

    PreferencesDialog pd(addNew, this);
    pd.exec();

    checkDriver();

    ui->config_window->loadConfigs();
    ui->config_window->unstashModifiedConfigs();

    settings.beginGroup("ide");
    if (!checkUpdates && settings.value("checkforupdates21",false).toBool()) {
        settings.setValue("lastCheck21",QDate::currentDate().addDays(-2).toString());
        IDE::instance()->checkUpdate();
    }

    indentSize = settings.value("indentSize", 2).toInt();
    useTabs = settings.value("indentTabs", false).toBool();
    IDE::instance()->setEditorIndent(indentSize, useTabs);
    bool wordWrap = settings.value("wordWrap", true).toBool();
    IDE::instance()->setEditorWordWrap(wordWrap ?
                                           QTextOption::WrapAtWordBoundaryOrAnywhere :
                                           QTextOption::NoWrap);

    settings.endGroup();

    settings.beginGroup("MainWindow");
    editorFont = IDEUtils::fontFromString(settings.value("editorFont").toString());
    auto zoom = settings.value("zoom", 100).toInt();
    editorFont.setPointSize(editorFont.pointSize() * zoom / 100);
    IDE::instance()->setEditorFont(editorFont);
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

void MainWindow::checkDriver() {
    auto& driver = MznDriver::get();
    bool haveMzn = (driver.isValid() && driver.solvers().size() > 0);
    ui->actionRun->setEnabled(haveMzn);
    ui->actionProfile_compilation->setEnabled(haveMzn);
    ui->actionCompile->setEnabled(haveMzn);
    ui->actionEditSolverConfig->setEnabled(haveMzn);
    ui->actionSubmit_to_MOOC->setEnabled(haveMzn);
    if (!haveMzn)
        ui->configWindow_dockWidget->hide();
}

void MainWindow::on_actionShift_left_triggered()
{
    if (curEditor != nullptr) {
        curEditor->shiftLeft();
    }
}

void MainWindow::on_actionShift_right_triggered()
{
    if (curEditor != nullptr) {
        curEditor->shiftRight();
    }
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
    if (getProject().hasProjectFile() || !getProject().files().empty()) {
        return false;
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
        IDE::instance()->projects.remove(p.projectFile());
        IDE::instance()->projects.insert(filepath, this);
        p.projectFile(filepath);
    }
    p.openTabsChanged(getOpenFiles(), ui->tabWidget->currentIndex());
    p.activeSolverConfigChanged(getCurrentSolverConfig());
    try {
        p.saveProject();
    } catch (FileError& f) {
        QMessageBox::critical(this, "MiniZinc IDE", f.message(), QMessageBox::Ok);
    }

    updateRecentProjects(p.projectFile());
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
        p.activeSolverConfigChanged(getCurrentSolverConfig());
        p.setModified(false);
        IDE::instance()->projects.insert(p.projectFile(), this);
        updateRecentProjects(p.projectFile());
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

    QRegularExpression comment("^(\\s*%|\\s*$)");
    QRegularExpression comSpace("%\\s");
    QRegularExpression emptyLine("^\\s*$");

    QTextBlock block = beginBlock;
    bool isCommented = true;
    do {
        if (!comment.match(block.text()).hasMatch()) {
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
                bool haveSpace = (comSpace.match(t,cpos).capturedStart() == cpos);
                cursor.movePosition(QTextCursor::Right,QTextCursor::KeepAnchor,haveSpace ? 2:1);
                cursor.removeSelectedText();
            }

        } else {
            if (!emptyLine.match(t).hasMatch())
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
    for (auto model : getProject().moocAssignment().models) {
        files.insert(model.model);
        files.insert(model.data);
    }
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
    connect(moocSubmission, &MOOCSubmission::finished, this, &MainWindow::moocFinished);
    setEnabled(false);
    moocSubmission->show();
}

void MainWindow::moocFinished(int) {
    moocSubmission->deleteLater();
    setEnabled(true);
}

bool MainWindow::eventFilter(QObject *obj, QEvent *ev)
{
    if (obj == ui->findWidget) {
        if (ev->type() == QEvent::KeyPress) {
            auto* keyEvent = static_cast<QKeyEvent*>(ev);
            if (keyEvent->key() == Qt::Key_Escape) {
                on_closeFindWidget_clicked();
                return true;
            }
        }
    }
    return QMainWindow::eventFilter(obj,ev);
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
    code_checker->start(contents, *sc, wd);
}

void MainWindow::setTheme(const Theme& theme, bool dark)
{
    darkMode = dark;
    for (int i=0; i<ui->tabWidget->count(); i++) {
        CodeEditor* ce = qobject_cast<CodeEditor*>(ui->tabWidget->widget(i));
        if (ce) {
            ce->setTheme(theme, darkMode);
        }
    }
    ui->outputWidget->setTheme(theme, darkMode);
    if (conductor != nullptr) {
        conductor->setDarkMode(darkMode);
    }
}

void MainWindow::initTheme()
{
    auto* tm = IDE::instance()->themeManager;
    auto* dm = IDE::instance()->darkModeNotifier;
    setTheme(tm->current(), dm->darkMode());
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
    closeFindWidget();
}

void MainWindow::closeFindWidget()
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
    ui->actionCompile->setEnabled(enabled);
    ui->actionProfile_compilation->setEnabled(enabled);
    ui->actionProfile_search->setEnabled(enabled);
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
    solverConfCombo->clear();
    solverConfCombo->addItems(items);
    solverConfCombo->setCurrentIndex(ui->config_window->currentIndex());
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

QList<CodeEditor*> MainWindow::codeEditors()
{
    QList<CodeEditor*> ret;
    for (int i=0; i<ui->tabWidget->count(); i++) {
        CodeEditor* ce = qobject_cast<CodeEditor*>(ui->tabWidget->widget(i));
        if (ce) {
            ret << ce;
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
            if (i != -1) {
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
    } else {
        int ret = QMessageBox::warning(this, "MiniZinc IDE",
                                       "Are you sure you wish to remove the selected file(s) from the project?",
                                       QMessageBox::Ok | QMessageBox::Cancel);
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

    getProject().remove(files);
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
        conductor->setDarkMode(darkMode);
        conductor->setWindowFlags(Qt::Widget);
        connect(conductor, &cpprofiler::Conductor::executionStart, [=] (cpprofiler::Execution* e) {
            ui->outputWidget->associateProfilerExecution(e->id());
        });
        connect(conductor, &cpprofiler::Conductor::showExecutionWindow, this, &MainWindow::showExecutionWindow);
        connect(conductor, &cpprofiler::Conductor::showMergeWindow, this, &MainWindow::showMergeWindow);
        profiler_layout->addWidget(conductor);
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
    QStringList args;
    args << "--cp-profiler"
         << QString::number(conductor->getNextExecId()) + "," + QString::number(conductor->getListenPort());
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
    ui->actionProfile_search->setDisabled(processRunning ||
                                          !curEditor ||
                                          !sc ||
                                          !sc->solverDefinition.stdFlags.contains("--cp-profiler"));
}

void MainWindow::on_progressOutput(float progress)
{
    progressBar->setHidden(false);
    progressBar->setValue(static_cast<int>(progress));
}

void MainWindow::on_minizincError(const QJsonObject& error) {
    bool isError = error["type"].toString() == "error";
    auto& currentTheme = IDE::instance()->themeManager->current();
    auto color = isError ? currentTheme.errorColor.get(darkMode)
                         : currentTheme.warningColor.get(darkMode);
    QString messageType = isError ? "Errors" : "Warnings";

    auto stack = error["stack"].toArray();
    if (!stack.empty()) {
        QString lastFile = "";
        int lastLine = -1;
        for (auto it : stack) {
            auto entry = it.toObject();
            auto loc = entry["location"].toObject();
            if (loc["filename"].toString() != lastFile || loc["firstLine"].toInt() != lastLine) {
                lastFile = loc["filename"].toString();
                lastLine = loc["firstLine"].toInt();

                ui->outputWidget->addHtml(locationToLink(
                                              loc["filename"].toString(),
                                              loc["firstLine"].toInt(),
                                              loc["firstColumn"].toInt(),
                                              loc["lastLine"].toInt(),
                                              loc["lastColumn"].toInt(),
                                              color
                                         ), messageType);
                ui->outputWidget->addText("\n", messageType);
            }
            ui->outputWidget->addText(QString(entry["isCompIter"].toBool() ? "    with " : "  in ") + entry["description"].toString(), messageType);
            ui->outputWidget->addText("\n", messageType);
        }
    } else if (error["location"].isObject()) {
        auto loc = error["location"].toObject();
        ui->outputWidget->addHtml(locationToLink(
                                      loc["filename"].toString(),
                                      loc["firstLine"].toInt(),
                                      loc["firstColumn"].toInt(),
                                      loc["lastLine"].toInt(),
                                      loc["lastColumn"].toInt(),
                                      color
                                 ), messageType);
        ui->outputWidget->addText(":\n", messageType);
    }

    QString what = error["what"].toString();
    QString message;

    if (error["includedFrom"].isArray()) {
        for (auto it : error["includedFrom"].toArray()) {
            message += QString(" (included from %1)\n").arg(it.toString());
        }
    }

    if (isError) {
        message += "MiniZinc: ";
    } else {
        message += "Warning: ";
    }
    if (error["what"].isString()) {
        message += QString("%1: ").arg(what);
    }
    message += QString("%1\n").arg(error["message"].toString());

    if (what == "cyclic include error") {
        for (auto it : error["cycle"].toArray()) {
            message += QString(" %1\n").arg(it.toString());
        }
    }

    ui->outputWidget->addText(message, messageType);
}

QString MainWindow::locationToLink(const QString& file, int firstLine, int firstColumn, int lastLine, int lastColumn, const QColor& color)
{
    QString label = QFileInfo(file).baseName();
    if (file.endsWith("untitled_model.mzn")) {
        QFileInfo fi(file);
        for (QTemporaryDir* d : cleanupTmpDirs) {
            QFileInfo tmpFileInfo(d->path() + "/untitled_model.mzn");
            if (fi.canonicalFilePath( )== tmpFileInfo.canonicalFilePath()) {
                label = "Playground";
                break;
            }
        }
    }
    QString position;
    if (firstLine == lastLine) {
        if (firstColumn == lastColumn) {
            position = QString("%1.%2")
                    .arg(firstLine)
                    .arg(firstColumn);
        } else {
            position = QString("%1.%2-%3")
                    .arg(firstLine)
                    .arg(firstColumn)
                    .arg(lastColumn);
        }
    } else {
        position = QString("%1-%2.%3-%4")
                .arg(firstLine)
                .arg(firstColumn)
                .arg(lastLine)
                .arg(lastColumn);
    }
    QUrl url = QUrl::fromLocalFile(file);
    url.setQuery(QString("line=%1&column=%2").arg(firstLine).arg(firstColumn));
    url.setScheme("err");
    return QString("<a style=\"color:%1\" href=\"%2\">%3:%4</a>")
                              .arg(color.name())
                              .arg(url.toString())
                              .arg(label)
                              .arg(position);
}

void MainWindow::startVisualisation(const QString& model, const QStringList& data, const QString& key, const QString& url, const QJsonValue& userData, MznProcess* proc)
{

    QSettings settings;
    settings.beginGroup("ide");
    bool reuseVis = settings.value("reuseVis", false).toBool();
    int httpPort = settings.value("visPort", 3000).toInt();
    int wsPort = settings.value("visWsPort", 3100).toInt();
    bool printUrl = settings.value("printVisUrl", false).toBool();
    settings.endGroup();

    if (server == nullptr) {
        server = new Server(this);
    }
    try {
        server->listen(httpPort, wsPort);
    } catch (ServerError& e) {
        QMessageBox::warning(this, "MiniZinc IDE", e.message(), QMessageBox::Ok);
        return;
    }

    QFileInfo modelFileInfo(model);
    QStringList files({modelFileInfo.fileName()});
    for (auto& df : data) {
        QFileInfo fi(df);
        files << fi.fileName();
    }
    auto workingDir = modelFileInfo.canonicalPath();

    auto label = files.join(", ");
    QStringList roots({workingDir, MznDriver::get().mznStdlibDir()});
    vis_connector = server->addConnector(label, roots);
    connect(vis_connector, &VisConnector::solveRequested, [=] (const QString& modelFile, bool dataFilesGiven, const QStringList& dataFiles, const QVariantMap& options) {
        auto* origSc = getCurrentSolverConfig();
        if (origSc == nullptr) {
            return;
        }
        QStringList df;
        bool modelFileGiven = !modelFile.isEmpty();
        auto m = modelFileGiven ? workingDir + "/" + modelFile : model;
        if (!IDEUtils::isChildPath(workingDir, m)) {
            return;
        }
        if (dataFilesGiven) {
            for (const auto& it : dataFiles) {
                auto d = workingDir + "/" + it;
                if (!IDEUtils::isChildPath(workingDir, d)) {
                    return;
                }
                df << d;
            }
        } else if (!modelFileGiven) {
            // No model, no data, so use previous
            df = data;
        }
        SolverConfiguration rsc(*origSc);
        QMapIterator<QString, QVariant> i(options);
        while (i.hasNext()) {
            i.next();
            rsc.extraOptions[i.key()] = i.value();
        }

        if (processRunning) {
            proc->terminate();
        }
        run(rsc, m, df);
    });
    connect(proc, &MznProcess::traceOutput, this, [=] (const QString& section, const QVariant& message) {
        if (section.startsWith("mzn_vis_")) {
            auto obj = message.toJsonObject();
            vis_connector->addWindow(section, obj["url"].toString(), obj["userData"]);
        }
    });
    connect(proc, &MznProcess::solutionOutput, [=](const QVariantMap& output, const QStringList& sections, qint64 time) {
        QJsonObject solution;
        for (auto it = output.begin(); it != output.end(); it++) {
            if (it.key().startsWith("mzn_vis_")) {
                solution[it.key()] = it.value().toJsonValue();
            }
        }
        vis_connector->addSolution(solution, time == -1 ? proc->elapsedTime() : time);
    });
    connect(proc, &MznProcess::finalStatus, [=](const QString& status, qint64 time) {
        vis_connector->setFinalStatus(status, time == -1 ? proc->elapsedTime() : time);
    });
    connect(proc, &MznProcess::finished, vis_connector, &VisConnector::setFinished);
    vis_connector->addWindow(key, url, userData);

    ui->outputWidget->associateServerUrl(vis_connector->url().toString());

    if (reuseVis) {
        QJsonObject obj({{"event", "navigate"}, {"url", vis_connector->url().toString()}});
        if (!server->sendToLastClient(QJsonDocument(obj))) {
            QDesktopServices::openUrl(vis_connector->url());
        }
    } else {
        QDesktopServices::openUrl(vis_connector->url());
    }

    if (printUrl) {
        auto url = vis_connector->url().toString();
        auto urlMessage = QString("<a href=\"%1\">%1</a>").arg(url);
        ui->outputWidget->addText("Visualisation: ", ui->outputWidget->infoCharFormat(), "Visualisation");
        ui->outputWidget->addHtml(urlMessage, "Visualisation");
        ui->outputWidget->addText("\n", ui->outputWidget->infoCharFormat(), "Visualisation");
    }
}
