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
#include <QtWebKitWidgets>
#include <QSet>

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "aboutdialog.h"
#include "codeeditor.h"
#include "webpage.h"
#include "fzndoc.h"
#include "finddialog.h"
#include "gotolinedialog.h"
#include "help.h"

#include <QtGlobal>
#ifdef Q_OS_WIN
#define pathSep ";"
#define MZN2FZN "mzn2fzn.bat"
#else
#define pathSep ":"
#define MZN2FZN "mzn2fzn"
#endif

struct IDE::Doc {
    QTextDocument td;
    QSet<CodeEditor*> editors;
    bool large;
    Doc() {
        td.setDocumentLayout(new QPlainTextDocumentLayout(&td));
    }
};

bool IDE::event(QEvent *e)
{
    switch (e->type()) {
    case QEvent::FileOpen:
    {
        QString file = static_cast<QFileOpenEvent*>(e)->file();
        if (file.endsWith(".mzp")) {
            PMap::iterator it = projects.find(file);
            if (it==projects.end()) {
                MainWindow* mw = static_cast<MainWindow*>(activeWindow());
                if (mw==NULL) {
                    mw = new MainWindow(file);
                    mw->show();
                } else {
                    mw->openProject(file);
                }
            } else {
                it.value()->raise();
                it.value()->activateWindow();
            }
        } else {
            QStringList files;
            files << file;
            MainWindow* mw = new MainWindow(files);
            MainWindow* curw = static_cast<MainWindow*>(activeWindow());
            if (curw!=NULL) {
                QPoint p = curw->pos();
                mw->move(p.x()+20, p.y()+20);
            }
            mw->show();
        }
        return true;
    }
    default:
        return QApplication::event(e);
    }
}

bool IDE::hasFile(const QString& path)
{
    return documents.find(path) != documents.end();
}

QTextDocument* IDE::addDocument(const QString& path, QTextDocument *doc, CodeEditor *ce)
{
    Doc* d = new Doc;
    d->td.setPlainText(doc->toPlainText());
    d->editors.insert(ce);
    d->large = false;
    documents.insert(path,d);
    return &d->td;
}

QPair<QTextDocument*,bool> IDE::loadFile(const QString& path, QWidget* parent)
{
    DMap::iterator it = documents.find(path);
    if (it==documents.end()) {
        QFile file(path);
        if (file.open(QFile::ReadOnly | QFile::Text)) {
            Doc* d = new Doc;
            if (path.endsWith(".dzn") && file.size() > 5*1024*1024) {
                d->large = true;
            } else {
                d->td.setPlainText(file.readAll());
                d->large = false;
            }
            d->td.setModified(false);
            documents.insert(path,d);
            return qMakePair(&d->td,d->large);
        } else {
            QMessageBox::warning(parent, "MiniZinc IDE",
                                 "Could not open file",
                                 QMessageBox::Ok);
            QTextDocument* nd = NULL;
            return qMakePair(nd,false);
        }

    } else {
        return qMakePair(&it.value()->td,it.value()->large);
    }
}

void IDE::loadLargeFile(const QString &path, QWidget* parent)
{
    DMap::iterator it = documents.find(path);
    if (it.value()->large) {
        QFile file(path);
        if (file.open(QFile::ReadOnly | QFile::Text)) {
            it.value()->td.setPlainText(file.readAll());
            it.value()->large = false;
            it.value()->td.setModified(false);
            QSet<CodeEditor*>::iterator ed = it.value()->editors.begin();
            for (; ed != it.value()->editors.end(); ++ed) {
                (*ed)->loadedLargeFile();
            }
        } else {
            QMessageBox::warning(parent, "MiniZinc IDE",
                                 "Could not open file",
                                 QMessageBox::Ok);
        }
    }
}

void IDE::registerEditor(const QString& path, CodeEditor* ce)
{
    DMap::iterator it = documents.find(path);
    QSet<CodeEditor*>& editors = it.value()->editors;
    editors.insert(ce);
}

void IDE::removeEditor(const QString& path, CodeEditor* ce)
{
    DMap::iterator it = documents.find(path);
    QSet<CodeEditor*>& editors = it.value()->editors;
    editors.remove(ce);
    if (editors.empty()) {
        delete it.value();
        documents.remove(path);
    }
}

IDE* MainWindow::ide()
{
    return static_cast<IDE*>(qApp);
}

MainWindow::MainWindow(const QString& project, QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    curEditor(NULL),
    process(NULL),
    tmpDir(NULL),
    saveBeforeRunning(false)
{
    init(project);
}

MainWindow::MainWindow(const QStringList& files, QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    curEditor(NULL),
    process(NULL),
    tmpDir(NULL),
    saveBeforeRunning(false)
{
    init(QString());
    for (int i=0; i<files.size(); i++)
        openFile(files.at(i),false);

}

void MainWindow::init(const QString& project)
{
    ui->setupUi(this);

    findDialog = new FindDialog(this);
    findDialog->setModal(false);

    helpWindow = new Help();

    connect(ui->tabWidget, SIGNAL(tabCloseRequested(int)), this, SLOT(tabCloseRequest(int)));
    connect(ui->tabWidget, SIGNAL(currentChanged(int)), this, SLOT(tabChange(int)));
    timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(statusTimerEvent()));
    solverTimeout = new QTimer(this);
    solverTimeout->setSingleShot(true);
    connect(solverTimeout, SIGNAL(timeout()), this, SLOT(on_actionStop_triggered()));
    statusLabel = new QLabel("");
    ui->statusbar->addPermanentWidget(statusLabel);
    ui->statusbar->showMessage("Ready.");
    ui->actionStop->setEnabled(false);
    QTabBar* tb = ui->tabWidget->findChild<QTabBar*>();
    tb->setTabButton(0, QTabBar::RightSide, 0);
    tabChange(0);
    tb->setTabButton(0, QTabBar::LeftSide, 0);

    connect(ui->outputConsole, SIGNAL(anchorClicked(QUrl)), this, SLOT(errorClicked(QUrl)));

    QSettings settings;
    settings.beginGroup("MainWindow");

    QFont defaultFont("Courier New");
    defaultFont.setStyleHint(QFont::Monospace);
    defaultFont.setPointSize(13);
    editorFont = settings.value("editorFont", defaultFont).value<QFont>();
    ui->outputConsole->setFont(editorFont);
    resize(settings.value("size", QSize(400, 400)).toSize());
    move(settings.value("pos", QPoint(200, 200)).toPoint());
    settings.endGroup();

    setEditorFont(editorFont);

    int nsolvers = settings.beginReadArray("solvers");
    if (nsolvers==0) {
        solvers.append(Solver("G12 fd","flatzinc","-Gg12_fd","",true));
        solvers.append(Solver("G12 lazyfd","flatzinc","-Gg12_fd","lazy",true));
        solvers.append(Solver("G12 CPX","fzn_cpx","-Gg12_cpx","",true));
        solvers.append(Solver("G12 MIP","flatzinc","-Glinear","mip",true));
    } else {
        for (int i=0; i<nsolvers; i++) {
            settings.setArrayIndex(i);
            Solver solver;
            solver.name = settings.value("name").toString();
            solver.executable = settings.value("executable").toString();
            solver.mznlib = settings.value("mznlib").toString();
            solver.backend = settings.value("backend").toString();
            solver.builtin = settings.value("builtin").toBool();
            solvers.append(solver);
        }
    }
    settings.endArray();
    settings.beginGroup("minizinc");
    mznDistribPath = settings.value("mznpath","").toString();
    defaultSolver = settings.value("defaultSolver","").toString();
    settings.endGroup();
    checkMznPath();
    for (int i=0; i<solvers.size(); i++)
        ui->conf_solver->addItem(solvers[i].name,i);
    if (!defaultSolver.isEmpty())
        ui->conf_solver->setCurrentText(defaultSolver);

    if (!project.isEmpty()) {
        loadProject(project);
    }
}

MainWindow::~MainWindow()
{
    if (process) {
        process->kill();
        process->waitForFinished();
        delete process;
    }
    delete ui;
    delete helpWindow;
    /// TODO: close tabs so that documents are unregistered
}

void MainWindow::on_actionNew_triggered()
{
    createEditor(QString(),false);
}

void MainWindow::createEditor(const QString& path, bool openAsModified) {
    QTextDocument* doc = NULL;
    bool large = false;
    QString fileContents;
    if (path.isEmpty()) {

    } else if (openAsModified) {
        QFile file(path);
        if (file.open(QFile::ReadOnly)) {
            fileContents = file.readAll();
        } else {
            QMessageBox::warning(this,"MiniZinc IDE",
                                 "Could not open file.",
                                 QMessageBox::Ok);
            return;
        }
    } else {
        QPair<QTextDocument*,bool> d = ide()->loadFile(path,this);
        doc = d.first;
        large = d.second;
    }
    if (doc || !fileContents.isEmpty() || path.isEmpty()) {
        CodeEditor* ce = new CodeEditor(doc,path,large,editorFont,ui->tabWidget,this);
        int tab = ui->tabWidget->addTab(ce, ce->filename);
        ui->tabWidget->setCurrentIndex(tab);
        curEditor->setFocus();
        if (openAsModified) {
            curEditor->filepath = "";
            curEditor->document()->setPlainText(fileContents);
            curEditor->document()->setModified(true);
            tabChange(ui->tabWidget->currentIndex());
        } else if (doc) {
            ide()->registerEditor(path,curEditor);
        }
        setupDznMenu();
    }
}

void MainWindow::openFile(const QString &path, bool openAsModified)
{
    QString fileName = path;

    if (fileName.isNull())
        fileName = QFileDialog::getOpenFileName(this, tr("Open File"), "", "MiniZinc Files (*.mzn *.dzn *.fzn)");

    if (!fileName.isEmpty()) {
        createEditor(fileName, openAsModified);
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
            on_actionSave_triggered();
            if (ce->document()->isModified())
                return;
        case QMessageBox::Discard:
            break;
        case QMessageBox::Cancel:
            return;
        default:
            return;
        }
    }
    ce->document()->setModified(false);
    setupDznMenu();
    ui->tabWidget->removeTab(tab);
    if (ui->tabWidget->count()==0) {
        on_actionNew_triggered();
    }
}

void MainWindow::closeEvent(QCloseEvent* e) {
    bool modified = false;
    for (int i=0; i<ui->tabWidget->count(); i++) {
        if (ui->tabWidget->widget(i) != ui->configuration &&
            static_cast<CodeEditor*>(ui->tabWidget->widget(i))->document()->isModified()) {
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
    if (process) {
        int ret = QMessageBox::warning(this, "MiniZinc IDE",
                                       "MiniZinc is currently running a solver.\nDo you want to quit anyway and stop the current process?",
                                       QMessageBox::Yes| QMessageBox::No);
        if (ret == QMessageBox::No) {
            e->ignore();
            return;
        }
    }
    if (process) {
        disconnect(process, SIGNAL(error(QProcess::ProcessError)),
                   this, SLOT(procError(QProcess::ProcessError)));
        process->kill();
    }
    for (int i=0; i<ui->tabWidget->count(); i++) {
        if (ui->tabWidget->widget(i) != ui->configuration) {
            CodeEditor* ce = static_cast<CodeEditor*>(ui->tabWidget->widget(i));
            ce->setDocument(NULL);
            if (ce->filepath != "")
                ide()->removeEditor(ce->filepath,ce);
        }
    }

    QSettings settings;
    settings.beginGroup("MainWindow");
    settings.setValue("editorFont", editorFont);
    settings.setValue("size", size());
    settings.setValue("pos", pos());
    settings.endGroup();
    helpWindow->close();
    e->accept();
}

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
        disconnect(curEditor->document(), SIGNAL(modificationChanged(bool)),
                   ui->actionSave, SLOT(setEnabled(bool)));
        disconnect(curEditor->document(), SIGNAL(undoAvailable(bool)),
                   ui->actionUndo, SLOT(setEnabled(bool)));
        disconnect(curEditor->document(), SIGNAL(redoAvailable(bool)),
                   ui->actionRedo, SLOT(setEnabled(bool)));
    }
    if (tab==-1) {
        curEditor = NULL;
        ui->actionClose->setEnabled(false);
    } else {
        if (ui->tabWidget->widget(tab)!=ui->configuration) {
            ui->actionClose->setEnabled(true);
            curEditor = static_cast<CodeEditor*>(ui->tabWidget->widget(tab));
            connect(ui->actionCopy, SIGNAL(triggered()), curEditor, SLOT(copy()));
            connect(ui->actionPaste, SIGNAL(triggered()), curEditor, SLOT(paste()));
            connect(ui->actionCut, SIGNAL(triggered()), curEditor, SLOT(cut()));
            connect(ui->actionUndo, SIGNAL(triggered()), curEditor, SLOT(undo()));
            connect(ui->actionRedo, SIGNAL(triggered()), curEditor, SLOT(redo()));
            connect(curEditor, SIGNAL(copyAvailable(bool)), ui->actionCopy, SLOT(setEnabled(bool)));
            connect(curEditor, SIGNAL(copyAvailable(bool)), ui->actionCut, SLOT(setEnabled(bool)));
            connect(curEditor->document(), SIGNAL(modificationChanged(bool)),
                    this, SLOT(setWindowModified(bool)));
            connect(curEditor->document(), SIGNAL(modificationChanged(bool)),
                    ui->actionSave, SLOT(setEnabled(bool)));
            connect(curEditor->document(), SIGNAL(undoAvailable(bool)),
                    ui->actionUndo, SLOT(setEnabled(bool)));
            connect(curEditor->document(), SIGNAL(redoAvailable(bool)),
                    ui->actionRedo, SLOT(setEnabled(bool)));
            setWindowModified(curEditor->document()->isModified());
            if (curEditor->filepath.isEmpty())
                setWindowFilePath(curEditor->filename);
            else
                setWindowFilePath(curEditor->filepath);
            ui->actionSave_as->setEnabled(true);
            ui->actionSave->setEnabled(curEditor->document()->isModified());
            ui->actionSelect_All->setEnabled(true);
            ui->actionUndo->setEnabled(curEditor->document()->isUndoAvailable());
            ui->actionRedo->setEnabled(curEditor->document()->isRedoAvailable());
            bool isMzn = QFileInfo(curEditor->filepath).completeSuffix()=="mzn";
            ui->actionRun->setEnabled(isMzn);
            ui->actionCompile->setEnabled(isMzn);
            bool isFzn = QFileInfo(curEditor->filepath).completeSuffix()=="fzn";
            ui->actionConstraint_Graph->setEnabled(isFzn);

            findDialog->setEditor(curEditor);
            ui->actionFind->setEnabled(true);
            ui->actionFind_next->setEnabled(true);
            ui->actionFind_previous->setEnabled(true);
            ui->actionReplace->setEnabled(true);
        } else {
            curEditor = NULL;
            ui->actionClose->setEnabled(false);
            ui->actionSave->setEnabled(false);
            ui->actionSave_as->setEnabled(false);
            ui->actionCut->setEnabled(false);
            ui->actionCopy->setEnabled(false);
            ui->actionPaste->setEnabled(false);
            ui->actionSelect_All->setEnabled(false);
            ui->actionUndo->setEnabled(false);
            ui->actionRedo->setEnabled(false);
            ui->actionRun->setEnabled(false);
            ui->actionCompile->setEnabled(false);
            ui->actionConstraint_Graph->setEnabled(false);
            ui->actionFind->setEnabled(false);
            ui->actionFind_next->setEnabled(false);
            ui->actionFind_previous->setEnabled(false);
            ui->actionReplace->setEnabled(false);
            findDialog->close();
            setWindowFilePath(projectPath);
            setWindowModified(false);
        }
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

QStringList MainWindow::parseConf(bool compileOnly)
{
    QStringList ret;
    if (!ui->conf_optimize->isChecked())
        ret << "--no-optimize";
    if (ui->conf_verbose->isChecked())
        ret << "-v";
    if (ui->conf_have_cmd_params->isChecked())
        ret << "-D"+ui->conf_cmd_params->text();
    if (ui->conf_data_file->currentText()!="None")
        ret << "-d"+ui->conf_data_file->currentText();
    if (!compileOnly && ui->conf_printall->isChecked())
        ret << "-a";
    if (!compileOnly && ui->conf_stats->isChecked())
        ret << "-s";
    if (!compileOnly && ui->conf_nthreads->value() != 1)
        ret << "-p"+QString::number(ui->conf_nthreads->value());
    if (!compileOnly && ui->conf_have_seed->isChecked())
        ret << "-r"+ui->conf_seed->text();
    if (!compileOnly && ui->conf_have_solverFlags->isChecked())
        ret << "--fzn-flags" << ui->conf_solverFlags->text();
    if (!compileOnly && ui->conf_nsol->value() != 1)
        ret << "-n"+QString::number(ui->conf_nsol->value());
    Solver s = solvers[ui->conf_solver->itemData(ui->conf_solver->currentIndex()).toInt()];
    if (!compileOnly) {
        ret << "-f" << s.executable;
        if (!s.backend.isEmpty())
            ret << "-b" << s.backend;
    }
    if (!s.mznlib.isEmpty())
        ret << s.mznlib;
    return ret;
}

void MainWindow::setupDznMenu()
{
    for (int i=0; i<ui->tabWidget->count(); i++) {
        if (ui->tabWidget->widget(i) != ui->configuration) {
            CodeEditor* ce = static_cast<CodeEditor*>(ui->tabWidget->widget(i));
            if (!ce->filepath.isEmpty() && ce->filepath.endsWith(".dzn") &&
                ui->conf_data_file->findText(ce->filepath) == -1) {
                    ui->conf_data_file->addItem(ce->filepath);
            }
        }
    }
}

void MainWindow::addOutput(const QString& s, bool html)
{
    QTextCursor cursor = ui->outputConsole->textCursor();
    cursor.movePosition(QTextCursor::End);
    ui->outputConsole->setTextCursor(cursor);
    if (html)
        ui->outputConsole->insertHtml(s);
    else
        ui->outputConsole->insertPlainText(s);
}

void MainWindow::on_actionRun_triggered()
{
    if (curEditor && curEditor->filepath!="") {
        if (curEditor->document()->isModified()) {
            if (!saveBeforeRunning) {
                QMessageBox msgBox;
                msgBox.setText("The model has been modified.");
                msgBox.setInformativeText("Do you want to save it before running?");
                QAbstractButton *saveButton = msgBox.addButton(QMessageBox::Save);
                msgBox.addButton(QMessageBox::Cancel);
                QAbstractButton *alwaysButton = msgBox.addButton("Always save", QMessageBox::AcceptRole);
                msgBox.setDefaultButton(QMessageBox::Save);
                msgBox.exec();
                if (msgBox.clickedButton()==alwaysButton) {
                    saveBeforeRunning = true;
                }
                if (msgBox.clickedButton()!=saveButton && msgBox.clickedButton()!=alwaysButton) {
                    return;
                }
            }
            on_actionSave_triggered();
        }
        if (curEditor->document()->isModified())
            return;
        ui->actionRun->setEnabled(false);
        ui->actionCompile->setEnabled(false);
        ui->actionStop->setEnabled(true);
        ui->configuration->setEnabled(false);
        process = new QProcess(this);
        process->setWorkingDirectory(QFileInfo(curEditor->filepath).absolutePath());
        connect(process, SIGNAL(readyReadStandardOutput()), this, SLOT(readOutput()));
        connect(process, SIGNAL(readyReadStandardError()), this, SLOT(readOutput()));
        connect(process, SIGNAL(finished(int)), this, SLOT(procFinished(int)));
        connect(process, SIGNAL(error(QProcess::ProcessError)),
                this, SLOT(procError(QProcess::ProcessError)));

        QStringList args = parseConf(false);
        args << QFileInfo(curEditor->filepath).fileName();
        if (ui->conf_have_timeLimit->isChecked()) {
            bool ok;
            int timeout = ui->conf_timeLimit->text().toInt(&ok);
            if (ok)
                solverTimeout->start(timeout*1000);
        }
        elapsedTime.start();
        addOutput("<div style='color:blue;'>Starting "+curEditor->filename+"</div><br>");
        if (!mznDistribPath.isEmpty()) {
            QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
            env.insert("PATH", env.value("PATH") + pathSep + mznDistribPath);
            process->setProcessEnvironment(env);
        }
        process->start(mznDistribPath+"minizinc",args);

        time = 0;
        timer->start(500);
    }
}

void MainWindow::setElapsedTime()
{
    int hours = elapsedTime.elapsed() / 3600000;
    int minutes = (elapsedTime.elapsed() % 3600000) / 60000;
    int seconds = (elapsedTime.elapsed() % 60000) / 1000;
    int msec = (elapsedTime.elapsed() % 1000);
    QString elapsed;
    if (hours > 0)
        elapsed += QString().number(hours)+"h ";
    if (hours > 0 || minutes > 0)
        elapsed += QString().number(minutes)+"m ";
    if (hours > 0 || minutes > 0 || seconds > 0)
        elapsed += QString().number(seconds)+"s";
    if (hours==0 && minutes==0)
        elapsed += " "+QString().number(msec)+"msec";
    statusLabel->setText(elapsed);
}

void MainWindow::statusTimerEvent()
{
    QString txt = "Running.";
    for (int i=time; i--;) txt+=".";
    ui->statusbar->showMessage(txt);
    time = (time+1) % 5;
    setElapsedTime();
}

void MainWindow::readOutput()
{
    process->setReadChannel(QProcess::StandardOutput);
    while (process->canReadLine()) {
        QString l = process->readLine();
//        if (l=="----------\n") {
//            addOutput("<br>");
//        } else if (l=="==========\n") {
//            addOutput("<div style='color:blue;'>Search complete.</div><br>");
//        } else {
            addOutput(l,false);
//        }
    }

    process->setReadChannel(QProcess::StandardError);
    while (process->canReadLine()) {
        QString l = process->readLine();
        QRegExp errexp("^(.*):([0-9]+):\\s*$");
        if (errexp.indexIn(l) != -1) {
            QUrl url = QUrl::fromLocalFile(errexp.cap(1));
            url.setQuery("line="+errexp.cap(2));
            url.setScheme("err");
            addOutput("<a style='color:red' href='"+url.toString()+"'>"+errexp.cap(1)+":"+errexp.cap(2)+":</a><br>");
        } else {
            addOutput(l,false);
        }
    }

}

void MainWindow::procFinished(int) {
    ui->actionRun->setEnabled(true);
    ui->actionCompile->setEnabled(true);
    ui->actionStop->setEnabled(false);
    ui->configuration->setEnabled(true);
    timer->stop();
    setElapsedTime();
    ui->statusbar->showMessage("Ready.");
    process = NULL;
}

void MainWindow::procError(QProcess::ProcessError e) {
    if (e==QProcess::FailedToStart) {
        QMessageBox::critical(this, "MiniZinc IDE", "Failed to start the MiniZinc interpreter. Check your path settings.");
    } else {
        QMessageBox::critical(this, "MiniZinc IDE", "Unknown error while executing the MiniZinc interpreter.");
    }
    process = NULL;
    ui->actionRun->setEnabled(true);
    ui->actionCompile->setEnabled(true);
}

void MainWindow::saveFile(CodeEditor* ce, const QString& f)
{
    QString filepath = f;
    int tabIndex = ui->tabWidget->indexOf(ce);
    if (filepath=="") {
        if (ce != curEditor) {
            ui->tabWidget->setCurrentIndex(tabIndex);
        }
        filepath = QFileDialog::getSaveFileName(this,"Save file",QString(),"MiniZinc files (*.mzn *.dzn *.fzn)");
    }
    if (!filepath.isEmpty()) {
        if (filepath != ce->filepath && ide()->hasFile(filepath)) {
            QMessageBox::warning(this,"MiniZinc IDE","Cannot overwrite open file.",
                                 QMessageBox::Ok);

        } else {
            QFile file(filepath);
            if (file.open(QFile::WriteOnly | QFile::Text)) {
                QTextStream out(&file);
                out << ce->document()->toPlainText();
                file.close();
                if (filepath != ce->filepath) {
                    QTextDocument* newdoc =
                            ide()->addDocument(filepath,ce->document(),ce);
                    ce->setDocument(newdoc);
                    if (ce->filepath != "") {
                        ide()->removeEditor(ce->filepath,ce);
                    }
                    setupDznMenu();
                }
                ce->document()->setModified(false);
                ce->filepath = filepath;
                ce->filename = QFileInfo(filepath).fileName();
                ui->tabWidget->setTabText(tabIndex,ce->filename);
                if (ce==curEditor)
                    tabChange(tabIndex);
            } else {
                QMessageBox::warning(this,"MiniZinc IDE","Could not save file");
            }
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
}

void MainWindow::on_actionStop_triggered()
{
    if (process) {
        disconnect(process, SIGNAL(error(QProcess::ProcessError)),
                   this, SLOT(procError(QProcess::ProcessError)));
        process->kill();
        process->waitForFinished();
        delete process;
        process = NULL;
        addOutput("<div style='color:blue;'>Stopped.</div><br>");
    }
}

void MainWindow::openCompiledFzn(int exitcode)
{
    if (exitcode==0) {
        openFile(currentFznTarget, true);
    }
    delete tmpDir;
    tmpDir = NULL;
}

void MainWindow::on_actionCompile_triggered()
{
    if (curEditor && curEditor->filepath!="") {
        ui->actionRun->setEnabled(false);
        ui->actionCompile->setEnabled(false);
        ui->actionStop->setEnabled(true);
        ui->configuration->setEnabled(false);
        process = new QProcess(this);
        process->setWorkingDirectory(QFileInfo(curEditor->filepath).absolutePath());
        process->setProcessChannelMode(QProcess::MergedChannels);
        connect(process, SIGNAL(readyRead()), this, SLOT(readOutput()));
        connect(process, SIGNAL(finished(int)), this, SLOT(procFinished(int)));
        connect(process, SIGNAL(finished(int)), this, SLOT(openCompiledFzn(int)));
        connect(process, SIGNAL(error(QProcess::ProcessError)),
                this, SLOT(procError(QProcess::ProcessError)));

        QStringList args = parseConf(true);

        tmpDir = new QTemporaryDir;
        if (!tmpDir->isValid()) {
            QMessageBox::critical(this, "MiniZinc IDE", "Could not create temporary directory for compilation.");
        } else {
            QFileInfo fi(curEditor->filepath);
            currentFznTarget = tmpDir->path()+"/"+fi.baseName()+".fzn";
            args << "-o" << currentFznTarget;
            args << QFileInfo(curEditor->filepath).fileName();
            addOutput("<div style='color:blue;'>Compiling "+curEditor->filename+"</div><br>");
            if (!mznDistribPath.isEmpty()) {
                QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
                env.insert("PATH", env.value("PATH") + pathSep + mznDistribPath);
                process->setProcessEnvironment(env);
            }
            process->start(mznDistribPath + MZN2FZN,args);
            time = 0;
            timer->start(500);
            elapsedTime.start();
        }
    }
}

void MainWindow::on_actionConstraint_Graph_triggered()
{
    
    WebPage* page = new WebPage();
    webView = new QWebView();
    webView->setPage(page);
    QString url = "qrc:/ConstraintGraph/index.html";
    connect(webView, SIGNAL(loadFinished(bool)), this, SLOT(webview_loaded(bool)));
    webView->load(url);
    webView->show();
}

void MainWindow::webview_loaded(bool ok) {
    if (ok){
        FznDoc fzndoc;
        fzndoc.setstr(curEditor->document()->toPlainText());
        webView->page()->mainFrame()->addToJavaScriptWindowObject("fznfile", &fzndoc);
        QString code = "start_s(fznfile)";
        webView->page()->mainFrame()->evaluateJavaScript(code);
    } else {
        qDebug() << "not ok";
    }
    
} 

void MainWindow::on_actionClear_output_triggered()
{
    ui->outputConsole->document()->clear();
}

void MainWindow::setEditorFont(QFont font)
{
    QTextCharFormat format;
    format.setFont(font);

    QTextCursor cursor(ui->outputConsole->document());
    cursor.movePosition(QTextCursor::Start);
    cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
    cursor.mergeCharFormat(format);
    for (int i=0; i<ui->tabWidget->count(); i++) {
        if (ui->tabWidget->widget(i)!=ui->configuration) {
            CodeEditor* ce = static_cast<CodeEditor*>(ui->tabWidget->widget(i));
            ce->setEditorFont(font);
        }
    }
}

void MainWindow::on_actionBigger_font_triggered()
{
    editorFont.setPointSize(editorFont.pointSize()+1);
    setEditorFont(editorFont);
}

void MainWindow::on_actionSmaller_font_triggered()
{
    editorFont.setPointSize(std::max(5, editorFont.pointSize()-1));
    setEditorFont(editorFont);
}

void MainWindow::on_actionDefault_font_size_triggered()
{
    editorFont.setPointSize(13);
    setEditorFont(editorFont);
}

void MainWindow::on_actionAbout_MiniZinc_IDE_triggered()
{
    AboutDialog().exec();
}

void MainWindow::errorClicked(const QUrl & url)
{
    for (int i=0; i<ui->tabWidget->count(); i++) {
        if (ui->tabWidget->widget(i) != ui->configuration) {
            CodeEditor* ce = static_cast<CodeEditor*>(ui->tabWidget->widget(i));
            if (ce->filepath == url.path()) {
                QRegExp re_line("line=([0-9]+)");
                if (re_line.indexIn(url.query()) != -1) {
                    bool ok;
                    int line = re_line.cap(1).toInt(&ok);
                    if (ok) {
                        QTextBlock block = ce->document()->findBlockByNumber(line-1);
                        if (block.isValid()) {
                            QTextCursor cursor = ce->textCursor();
                            cursor.setPosition(block.position());
                            ce->setFocus();
                            ce->setTextCursor(cursor);
                            ui->tabWidget->setCurrentIndex(i);
                        }
                    }
                }

            }
        }
    }
}

void MainWindow::on_actionManage_solvers_triggered()
{
    SolverDialog sd(solvers,defaultSolver,mznDistribPath);
    sd.exec();
    defaultSolver = sd.def();
    mznDistribPath = sd.mznPath();
    if (! (mznDistribPath.endsWith("/") || mznDistribPath.endsWith("\\")))
        mznDistribPath += "/";
    checkMznPath();
    QString curSelection = ui->conf_solver->currentText();
    ui->conf_solver->clear();
    for (int i=0; i<solvers.size(); i++)
        ui->conf_solver->addItem(solvers[i].name,i);
    int idx = ui->conf_solver->findText(curSelection);
    if (idx!=-1)
        ui->conf_solver->setCurrentIndex(idx);
    else
        ui->conf_solver->setCurrentText(defaultSolver);

    QSettings settings;
    settings.beginGroup("minizinc");
    settings.setValue("mznpath",mznDistribPath);
    settings.setValue("defaultSolver",defaultSolver);
    settings.endGroup();

    settings.beginWriteArray("solvers");
    for (int i=0; i<solvers.size(); i++) {
        settings.setArrayIndex(i);
        settings.setValue("name",solvers[i].name);
        settings.setValue("executable",solvers[i].executable);
        settings.setValue("mznlib",solvers[i].mznlib);
        settings.setValue("backend",solvers[i].backend);
        settings.setValue("builtin",solvers[i].builtin);
    }
    settings.endArray();
}

void MainWindow::on_actionFind_triggered()
{
    findDialog->show();
}

void MainWindow::on_actionReplace_triggered()
{
    findDialog->show();
}

void MainWindow::on_actionSelect_font_triggered()
{
    bool ok;
    QFont newFont = QFontDialog::getFont(&ok,editorFont,this);
    if (ok) {
        editorFont = newFont;
        setEditorFont(editorFont);
    }
}

void MainWindow::on_actionGo_to_line_triggered()
{
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

void MainWindow::checkMznPath()
{
    QProcess p;
    QStringList args;
    args << "-v";
    p.start(mznDistribPath+"minizinc", args);
    if (!p.waitForStarted() || !p.waitForFinished()) {
        int ret = QMessageBox::warning(this,"MiniZinc IDE","Could not find the minizinc executable.\nPlease check the path settings in the solver menu.",
                                       QMessageBox::Ok | QMessageBox::Cancel);
        if (ret == QMessageBox::Ok)
            on_actionManage_solvers_triggered();
    }
}

void MainWindow::on_actionOnly_editor_triggered()
{
    QList<int> sizes;
    sizes << 100;
    sizes << 0;
    ui->splitter->setSizes(sizes);
}

void MainWindow::on_actionOnly_output_triggered()
{
    QList<int> sizes;
    sizes << 0;
    sizes << 100;
    ui->splitter->setSizes(sizes);
}

void MainWindow::on_actionSplit_triggered()
{
    QList<int> sizes;
    sizes << ui->splitter->height()/3*2;
    sizes << ui->splitter->height()/3;
    ui->splitter->setSizes(sizes);
}

void MainWindow::on_actionShift_left_triggered()
{
    QTextCursor cursor = curEditor->textCursor();
    QTextBlock block = curEditor->document()->findBlock(cursor.anchor());
    QRegExp white("\\s");
    QTextBlock endblock = curEditor->document()->findBlock(cursor.position()).next();
    cursor.beginEditBlock();
    do {
        cursor.setPosition(block.position());
        if (block.length() > 2) {
            cursor.movePosition(QTextCursor::Right,QTextCursor::KeepAnchor,2);
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
    QTextBlock block = curEditor->document()->findBlock(cursor.anchor());
    QTextBlock endblock = curEditor->document()->findBlock(cursor.position()).next();
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
    helpWindow->show();
}

void MainWindow::on_actionNew_project_triggered()
{
    MainWindow* mw = new MainWindow;
    QPoint p = pos();
    mw->move(p.x()+20, p.y()+20);
    mw->show();
}

void MainWindow::openProject(const QString& fileName)
{
    if (!fileName.isEmpty()) {
        IDE::PMap& pmap = ide()->projects;
        IDE::PMap::iterator it = pmap.find(fileName);
        if (it==pmap.end()) {
            if (ui->tabWidget->count()==1) {
                loadProject(fileName);
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

void MainWindow::on_actionOpen_project_triggered()
{
    QString fileName = QFileDialog::getOpenFileName(this, tr("Open Project"), "", "MiniZinc projects (*.mzp)");
    openProject(fileName);
}

void MainWindow::saveProject(const QString& f)
{
    QString filepath = f;
    if (filepath.isEmpty()) {
        filepath = QFileDialog::getSaveFileName(this,"Save project",QString(),"MiniZinc projects (*.mzp)");
    }
    if (!filepath.isEmpty()) {
        if (projectPath != filepath && ide()->projects.contains(filepath)) {
            QMessageBox::warning(this,"MiniZinc IDE","Cannot overwrite existing open project.",
                                 QMessageBox::Ok);
            return;
        }
        QFile file(filepath);
        if (file.open(QFile::WriteOnly | QFile::Text)) {
            if (projectPath != filepath) {
                ide()->projects.remove(projectPath);
                ide()->projects.insert(filepath,this);
            }
            projectPath = filepath;
            QDataStream out(&file);
            out << (quint32)0xD539EA12;
            out << (quint32)101;
            out.setVersion(QDataStream::Qt_5_0);
            QStringList openFiles;
            for (int i=0; i<ui->tabWidget->count(); i++) {
                if (ui->tabWidget->widget(i)!=ui->configuration) {
                    CodeEditor* ce = static_cast<CodeEditor*>(ui->tabWidget->widget(i));
                    if (!ce->filepath.isEmpty())
                        openFiles << ce->filepath;
                }
            }
            out << openFiles;

            out << ui->conf_include_path->text();
            out << (qint32)ui->conf_data_file->currentIndex();
            out << ui->conf_have_cmd_params->isChecked();
            out << ui->conf_cmd_params->text();
            out << ui->conf_verbose->isChecked();
            out << ui->conf_optimize->isChecked();
            out << ui->conf_solver->currentText();
            out << (qint32)ui->conf_nsol->value();
            out << ui->conf_printall->isChecked();
            out << ui->conf_stats->isChecked();
            out << ui->conf_have_solverFlags->isChecked();
            out << ui->conf_solverFlags->text();
            out << (qint32)ui->conf_nthreads->value();
            out << ui->conf_have_seed->isChecked();
            out << ui->conf_seed->text();
            out << ui->conf_have_timeLimit->isChecked();
            out << (qint32)ui->conf_timeLimit->value();
        } else {
            QMessageBox::warning(this,"MiniZinc IDE","Could not save project");
        }
    }
}

void MainWindow::loadProject(const QString& filepath)
{
    QFile pfile(filepath);
    pfile.open(QIODevice::ReadOnly);
    QDataStream in(&pfile);
    quint32 magic;
    in >> magic;
    if (magic != 0xD539EA12) {
        QMessageBox::warning(this, "MiniZinc IDE",
                             "Could not open project file");
        close();
        return;
    }
    quint32 version;
    in >> version;
    if (version != 101) {
        QMessageBox::warning(this, "MiniZinc IDE",
                             "Could not open project file (version mismatch)");
        close();
        return;
    }
    in.setVersion(QDataStream::Qt_5_0);

    QStringList openFiles;
    in >> openFiles;
    for (int i=0; i<openFiles.size(); i++) {
        openFile(openFiles[i],false);
    }
    QString p_s;
    qint32 p_i;
    bool p_b;

    in >> p_s;
    ui->conf_include_path->setText(p_s);
    in >> p_i;
    ui->conf_data_file->setCurrentIndex(p_i);
    in >> p_b;
    ui->conf_have_cmd_params->setChecked(p_b);
    in >> p_s;
    ui->conf_cmd_params->setText(p_s);
    in >> p_b;
    ui->conf_verbose->setChecked(p_b);
    in >> p_b;
    ui->conf_optimize->setChecked(p_b);
    in >> p_s;
    ui->conf_solver->setCurrentText(p_s);
    in >> p_i;
    ui->conf_nsol->setValue(p_i);
    in >> p_b;
    ui->conf_printall->setChecked(p_b);
    in >> p_b;
    ui->conf_stats->setChecked(p_b);
    in >> p_b;
    ui->conf_have_solverFlags->setChecked(p_b);
    in >> p_s;
    ui->conf_solverFlags->setText(p_s);
    in >> p_i;
    ui->conf_nthreads->setValue(p_i);
    in >> p_b;
    ui->conf_have_seed->setChecked(p_b);
    in >> p_s;
    ui->conf_seed->setText(p_s);
    in >> p_b;
    ui->conf_have_timeLimit->setChecked(p_b);
    in >> p_i;
    ui->conf_timeLimit->setValue(p_i);
    projectPath = filepath;

    ide()->projects.insert(projectPath, this);

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
    if (!projectPath.isEmpty())
        ide()->projects.remove(projectPath);
    close();
}

void MainWindow::on_actionFind_next_triggered()
{
    findDialog->on_b_next_clicked();
}

void MainWindow::on_actionFind_previous_triggered()
{
    findDialog->on_b_prev_clicked();
}

void MainWindow::on_actionSave_all_triggered()
{
    for (int i=0; i<ui->tabWidget->count(); i++) {
        if (ui->tabWidget->widget(i)!=ui->configuration) {
            CodeEditor* ce = static_cast<CodeEditor*>(ui->tabWidget->widget(i));
            if (ce->document()->isModified())
                saveFile(ce,ce->filepath);
        }
    }
}
