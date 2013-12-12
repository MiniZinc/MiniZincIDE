#include <QtWidgets>

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "codeeditor.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    process(NULL)
{
    ui->setupUi(this);
    QFont font("Courier New");
    font.setStyleHint(QFont::Monospace);
    ui->outputConsole->setFont(font);
    connect(ui->tabWidget, SIGNAL(tabCloseRequested(int)), this, SLOT(tabCloseRequest(int)));
    connect(ui->tabWidget, SIGNAL(currentChanged(int)), this, SLOT(tabChange(int)));
    QFile file;
    createEditor(file);
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
    createEditor(file);
}


void MainWindow::createEditor(QFile& file) {
    if (curEditor && curEditor->filepath=="" && !curEditor->document()->isModified()) {
        if (file.isOpen()) {
            curEditor->setPlainText(file.readAll());
            curEditor->filepath = QFileInfo(file).absoluteFilePath();
            curEditor->filename = QFileInfo(file).fileName();
            ui->tabWidget->setTabText(ui->tabWidget->currentIndex(),curEditor->filename);
        }
    } else {
        CodeEditor* ce = new CodeEditor(file,this);
        int tab = ui->tabWidget->addTab(ce, ce->filename);
        ui->tabWidget->setCurrentIndex(tab);
    }
}

void MainWindow::openFile(const QString &path)
{
    QString fileName = path;

    if (fileName.isNull())
        fileName = QFileDialog::getOpenFileName(this, tr("Open File"), "", "MiniZinc Files (*.mzn *.dzn)");

    if (!fileName.isEmpty()) {
        QFile file(fileName);
        if (file.open(QFile::ReadOnly | QFile::Text)) {
            createEditor(file);
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
    ui->tabWidget->removeTab(tab);
    if (ui->tabWidget->count()==0) {
        on_actionNew_triggered();
    }
}

void MainWindow::closeEvent(QCloseEvent* e) {
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
    } else {
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
        ui->actionSave->setEnabled(curEditor->document()->isModified());
        ui->actionUndo->setEnabled(curEditor->document()->isUndoAvailable());
        ui->actionRedo->setEnabled(curEditor->document()->isRedoAvailable());
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

void MainWindow::on_actionRun_triggered()
{
    if (curEditor && curEditor->filepath!="") {
        ui->actionRun->setEnabled(false);
        process = new QProcess(this);
        process->setWorkingDirectory(QFileInfo(curEditor->filepath).absolutePath());
        process->setProcessChannelMode(QProcess::MergedChannels);
        connect(process, SIGNAL(readyRead()), this, SLOT(readOutput()));
        connect(process, SIGNAL(finished(int)), this, SLOT(procFinished(int)));
        connect(process, SIGNAL(error(QProcess::ProcessError)),
                this, SLOT(procError(QProcess::ProcessError)));

        QStringList args;
        args << curEditor->filepath;
        ui->outputConsole->insertHtml("<div style='color:red;'>Starting "+curEditor->filename+"</div><br>");
        process->start("mzn-gecode",args);
    }
}

void MainWindow::readOutput()
{
    while (process->canReadLine()) {
        QString l = process->readLine();
        if (l=="----------\n") {
            ui->outputConsole->insertHtml("<br>");
        } else if (l=="==========\n") {
            ui->outputConsole->insertHtml("<div style='color:red;'>Search complete.</div><br>");
        } else {
            ui->outputConsole->insertPlainText(l);
        }
    }
}

void MainWindow::procFinished(int) {
    ui->actionRun->setEnabled(true);
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
}

void MainWindow::on_actionSave_triggered()
{
    if (curEditor) {
        QString filepath = curEditor->filepath;
        if (filepath=="") {
            filepath = QFileDialog::getSaveFileName(this,"Save file",QString(),"MiniZinc files (*.mzn *.dzn *.fzn)");
        }
        if (!filepath.isEmpty()) {
            QFile file(filepath);
            if (file.open(QFile::WriteOnly | QFile::Text)) {
                QTextStream out(&file);
                out << curEditor->document()->toPlainText();
                file.close();
                curEditor->document()->setModified(false);
                curEditor->filepath = filepath;
                curEditor->filename = QFileInfo(filepath).fileName();
                ui->tabWidget->setTabText(ui->tabWidget->currentIndex(),curEditor->filename);
            }
        }
    }
}

void MainWindow::on_actionQuit_triggered()
{
    close();
}
