#include <QtWidgets>
#include <QApplication>
#include <QtWebKitWidgets>

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "aboutdialog.h"
#include "codeeditor.h"
#include "webpage.h"
#include "fzndoc.h"
#include "finddialog.h"
#include "findreplacedialog.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    curEditor(NULL),
    process(NULL),
    tmpDir(NULL)
{
    ui->setupUi(this);

    findDialog = new FindDialog(this);
    findDialog->setModal(false);
    findReplaceDialog = new FindReplaceDialog(this);
    findReplaceDialog->setModal(false);

    QFont font("Courier New");
    font.setStyleHint(QFont::Monospace);
    ui->outputConsole->setFont(font);

    connect(ui->tabWidget, SIGNAL(tabCloseRequested(int)), this, SLOT(tabCloseRequest(int)));
    connect(ui->tabWidget, SIGNAL(currentChanged(int)), this, SLOT(tabChange(int)));
    timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(statusTimerEvent()));
    statusLabel = new QLabel("Ready");
    ui->statusbar->addWidget(statusLabel);
    ui->actionStop->setEnabled(false);
    QTabBar* tb = ui->tabWidget->findChild<QTabBar*>();
    tb->setTabButton(0, QTabBar::RightSide, 0);
    tabChange(0);
    tb->setTabButton(0, QTabBar::LeftSide, 0);

    connect(ui->outputConsole, SIGNAL(anchorClicked(QUrl)), this, SLOT(errorClicked(QUrl)));

    QSettings settings;
    settings.beginGroup("MainWindow");
    fontSize = settings.value("fontSize", 13.0).toDouble();
    resize(settings.value("size", QSize(400, 400)).toSize());
    move(settings.value("pos", QPoint(200, 200)).toPoint());
    settings.endGroup();

    setFontSize(fontSize);

    findDialog->readSettings(settings);
    findReplaceDialog->readSettings(settings);

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
    for (int i=0; i<solvers.size(); i++)
        ui->conf_solver->addItem(solvers[i].name,i);

    QStringList args = QApplication::arguments();
    for (int i=1; i<args.size(); i++)
        openFile(args.at(i),false);

}

MainWindow::~MainWindow()
{
    if (process) {
        process->kill();
        process->waitForFinished();
        delete process;
    }
    delete ui;
}

void MainWindow::on_actionNew_triggered()
{
    QFile file;
    createEditor(file,false);
}

void MainWindow::createEditor(QFile& file, bool openAsModified) {
    if (curEditor && curEditor->filepath=="" && !curEditor->document()->isModified()) {
        if (file.isOpen()) {
            curEditor->setPlainText(file.readAll());
            curEditor->filepath = QFileInfo(file).absoluteFilePath();
            curEditor->filename = QFileInfo(file).fileName();
            ui->tabWidget->setTabText(ui->tabWidget->currentIndex(),curEditor->filename);
            curEditor->setFocus();
        }
    } else {
        CodeEditor* ce = new CodeEditor(file,fontSize,this);
        int tab = ui->tabWidget->addTab(ce, ce->filename);
        ui->tabWidget->setCurrentIndex(tab);
        curEditor->setFocus();
    }
    if (openAsModified) {
        curEditor->filepath = "";
        curEditor->document()->setModified(true);
        tabChange(ui->tabWidget->currentIndex());
    } else {
        addFile(curEditor->filepath);
    }
}

void MainWindow::openFile(const QString &path, bool openAsModified)
{
    QString fileName = path;

    if (fileName.isNull())
        fileName = QFileDialog::getOpenFileName(this, tr("Open File"), "", "MiniZinc Files (*.mzn *.dzn *.fzn)");

    if (!fileName.isEmpty()) {
        QFile file(fileName);
        if (file.open(QFile::ReadOnly | QFile::Text)) {
            createEditor(file, openAsModified);
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
    removeFile(ce->filepath);
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
    QSettings settings;
    settings.beginGroup("MainWindow");
    settings.setValue("fontSize", fontSize);
    settings.setValue("size", size());
    settings.setValue("pos", pos());
    settings.endGroup();
    findDialog->writeSettings(settings);
    findReplaceDialog->writeSettings(settings);
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

            findDialog->setTextEdit(curEditor);
            findReplaceDialog->setTextEdit(curEditor);
            ui->actionFind->setEnabled(true);
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
            ui->actionReplace->setEnabled(false);
            findDialog->close();
            findReplaceDialog->close();
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

void MainWindow::addFile(const QString &path)
{
    if (!path.isEmpty() && !filePaths.contains(path)) {
        filePaths.insert(path);
        if (path.endsWith(".dzn"))
            ui->conf_data_file->addItem(path);
    }
}

void MainWindow::removeFile(const QString& path)
{
    filePaths.remove(path);
    if (path.endsWith(".dzn")) {
        ui->conf_data_file->removeItem(ui->conf_data_file->findText(path));
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
        args << curEditor->filepath;
        addOutput("<div style='color:blue;'>Starting "+curEditor->filename+"</div><br>");
        process->start("minizinc",args);
        time = 0;
        timer->start(500);
    }
}

void MainWindow::statusTimerEvent()
{
    QString txt = "Running.";
    for (int i=time; i--;) txt+=".";
    statusLabel->setText(txt);
    time = (time+1) % 5;
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
    statusLabel->setText("Ready");
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

void MainWindow::saveFile(const QString& f)
{
    QString filepath = f;
    if (filepath=="") {
        filepath = QFileDialog::getSaveFileName(this,"Save file",QString(),"MiniZinc files (*.mzn *.dzn *.fzn)");
    }
    if (!filepath.isEmpty()) {
        QFile file(filepath);
        if (file.open(QFile::WriteOnly | QFile::Text)) {
            QTextStream out(&file);
            out << curEditor->document()->toPlainText();
            file.close();
            if (filepath != curEditor->filepath) {
                removeFile(curEditor->filepath);
                addFile(filepath);
            }
            curEditor->document()->setModified(false);
            curEditor->filepath = filepath;
            curEditor->filename = QFileInfo(filepath).fileName();
            ui->tabWidget->setTabText(ui->tabWidget->currentIndex(),curEditor->filename);
            tabChange(ui->tabWidget->currentIndex());
        }
    }
}

void MainWindow::on_actionSave_triggered()
{
    if (curEditor) {
        saveFile(curEditor->filepath);
    }
}

void MainWindow::on_actionSave_as_triggered()
{
    if (curEditor) {
        saveFile(QString());
    }
}

void MainWindow::on_actionQuit_triggered()
{
    close();
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
            args << curEditor->filepath;
            addOutput("<div style='color:blue;'>Compiling "+curEditor->filename+"</div><br>");
            process->start("mzn2fzn",args);
            time = 0;
            timer->start(500);
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

void MainWindow::setFontSize(double points)
{
    QTextCharFormat format;
    format.setFontPointSize(points);

    QTextCursor cursor(ui->outputConsole->document());
    cursor.movePosition(QTextCursor::Start);
    cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
    cursor.mergeCharFormat(format);
    for (int i=0; i<ui->tabWidget->count(); i++) {
        if (ui->tabWidget->widget(i)!=ui->configuration) {
            CodeEditor* ce = static_cast<CodeEditor*>(ui->tabWidget->widget(i));
            ce->setFontSize(points);
        }
    }
}

void MainWindow::on_actionBigger_font_triggered()
{
    fontSize += 1.0;
    setFontSize(fontSize);
}

void MainWindow::on_actionSmaller_font_triggered()
{
    fontSize = std::max(5.0, fontSize-1.0);
    setFontSize(fontSize);
}

void MainWindow::on_actionDefault_font_size_triggered()
{
    fontSize = 13.0;
    setFontSize(fontSize);
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
                        QTextBlock block = ce->document()->findBlockByLineNumber(line-1);
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

void MainWindow::on_actionManage_solvers_triggered()
{
    SolverDialog sd(solvers);
    sd.exec();
    ui->conf_solver->clear();
    for (int i=0; i<solvers.size(); i++)
        ui->conf_solver->addItem(solvers[i].name,i);
    QSettings settings;
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
    findReplaceDialog->show();
}
