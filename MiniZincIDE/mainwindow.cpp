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
#include <csignal>
#include <sstream>

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "codeeditor.h"
#include "fzndoc.h"
#include "gotolinedialog.h"
#include "paramdialog.h"
#include "checkupdatedialog.h"
#include "moocsubmission.h"
#include "highlighter.h"

#include <QtGlobal>
#ifdef Q_OS_WIN
#define pathSep ";"
#define fileDialogSuffix "/"
#define MZNOS "win"
#else
#define pathSep ":"
#ifdef Q_OS_MAC
#define fileDialogSuffix "/*"
#define MZNOS "mac"
#include "macos_extras.h"
#else
#define fileDialogSuffix "/"
#define MZNOS "linux"
#endif
#endif

IDEStatistics::IDEStatistics(void)
    : errorsShown(0), errorsClicked(0), modelsRun(0) {}

void IDEStatistics::init(QVariant v) {
    if (v.isValid()) {
        QMap<QString,QVariant> m = v.toMap();
        errorsShown = m["errorsShown"].toInt();
        errorsClicked = m["errorsClicked"].toInt();
        modelsRun = m["modelsRun"].toInt();
        solvers = m["solvers"].toStringList();
    }
#ifdef Q_OS_MAC
    (void) new MyRtfMime();
#endif
}

QVariantMap IDEStatistics::toVariantMap(void) {
    QMap<QString,QVariant> m;
    m["errorsShown"] = errorsShown;
    m["errorsClicked"] = errorsClicked;
    m["modelsRun"] = modelsRun;
    m["solvers"] = solvers;
    return m;
}

QByteArray IDEStatistics::toJson(void) {
    QJsonDocument jd(QJsonObject::fromVariantMap(toVariantMap()));
    return jd.toJson();
}

void IDEStatistics::resetCounts(void) {
    errorsShown = 0;
    errorsClicked = 0;
    modelsRun = 0;
}

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
                MainWindow* mw = qobject_cast<MainWindow*>(activeWindow());
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
            MainWindow* curw = qobject_cast<MainWindow*>(activeWindow());
            if (curw != NULL && (curw->isEmptyProject() || curw==lastDefaultProject)) {
                curw->openFile(file);
                lastDefaultProject = curw;
            } else {
                QStringList files;
                files << file;
                MainWindow* mw = new MainWindow(files);
                if (curw!=NULL) {
                    QPoint p = curw->pos();
                    mw->move(p.x()+20, p.y()+20);
                }
                mw->show();
                lastDefaultProject = mw;
            }
        }
        return true;
    }
    default:
        return QApplication::event(e);
    }
}

void IDE::checkUpdate(void) {
    QSettings settings;
    settings.sync();

    settings.beginGroup("ide");
    if (settings.value("checkforupdates21",false).toBool()) {
        QDate lastCheck = QDate::fromString(settings.value("lastCheck21",
                                                           QDate::currentDate().addDays(-2).toString()).toString());
        if (lastCheck < QDate::currentDate()) {
            // Prepare Google Analytics event
            QUrlQuery gaQuery;
            gaQuery.addQueryItem("v","1"); // version 1 of the protocol
            gaQuery.addQueryItem("tid","UA-63390311-1"); // the MiniZinc ID
            gaQuery.addQueryItem("cid",settings.value("uuid","unknown").toString()); // identifier for this installation
            gaQuery.addQueryItem("aip","1"); // anonymize IP address
            gaQuery.addQueryItem("t","event"); // feedback type
            gaQuery.addQueryItem("ec","check"); // event type
            gaQuery.addQueryItem("ea","checkUpdate"); // event action
            gaQuery.addQueryItem("el",applicationVersion()); // event label (IDE version)
            QNetworkRequest gaRequest(QUrl("http://www.google-analytics.com/collect"));
            gaRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
            networkManager->post(gaRequest, gaQuery.toString().toLocal8Bit());

            // Check if an update is available
            QNetworkRequest request(QUrl("http://www.minizinc.org/version-info.php"));
            versionCheckReply = networkManager->get(request);
            connect(versionCheckReply, SIGNAL(finished()), this, SLOT(versionCheckFinished()));
        }
        QTimer::singleShot(24*60*60*1000, this, SLOT(checkUpdate()));
    }
    settings.endGroup();
}


IDE::IDE(int& argc, char* argv[]) : QApplication(argc,argv) {
    setApplicationVersion(MINIZINC_IDE_VERSION);
    setOrganizationName("MiniZinc");
    setOrganizationDomain("minizinc.org");
#ifndef MINIZINC_IDE_BUNDLED
    setApplicationName("MiniZinc IDE");
#else
    setApplicationName("MiniZinc IDE (bundled)");
#endif

    networkManager = new QNetworkAccessManager(this);

    setAttribute(Qt::AA_UseHighDpiPixmaps);

    QSettings settings;
    settings.sync();
    settings.beginGroup("ide");
    if (settings.value("lastCheck21",QString()).toString().isEmpty()) {
        settings.setValue("uuid", QUuid::createUuid().toString());

        CheckUpdateDialog cud;
        int result = cud.exec();

        settings.setValue("lastCheck21",QDate::currentDate().addDays(-2).toString());
        settings.setValue("checkforupdates21",result==QDialog::Accepted);
        settings.sync();
    }
    settings.endGroup();
    settings.beginGroup("Recent");
    recentFiles = settings.value("files",QStringList()).toStringList();
    recentProjects = settings.value("projects",QStringList()).toStringList();
    settings.endGroup();

    stats.init(settings.value("statistics"));

    lastDefaultProject = NULL;

    { // Load cheat sheet
        QString fileContents;
        QFile file(":/cheat_sheet.mzn");
        if (file.open(QFile::ReadOnly)) {
            fileContents = file.readAll();
        } else {
            qDebug() << "internal error: cannot open cheat sheet.";
        }

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
        QFont editorFont;
        editorFont.fromString(settings.value("editorFont", defaultFont.toString()).value<QString>());
        bool darkMode = settings.value("darkMode", false).value<bool>();
        settings.endGroup();

        cheatSheet = new QMainWindow;
        cheatSheet->setWindowTitle("MiniZinc Cheat Sheet");
        CodeEditor* ce = new CodeEditor(NULL,":/cheat_sheet.mzn",false,false,editorFont,darkMode,NULL,NULL);
        ce->document()->setPlainText(fileContents);
        QTextCursor cursor = ce->textCursor();
        cursor.movePosition(QTextCursor::Start);
        ce->setTextCursor(cursor);

        ce->setReadOnly(true);
        cheatSheet->setCentralWidget(ce);
        cheatSheet->resize(800, 600);
    }


    connect(&fsWatch, SIGNAL(fileChanged(QString)), this, SLOT(fileModified(QString)));
    connect(this, SIGNAL(focusChanged(QWidget*,QWidget*)), this, SLOT(handleFocusChange(QWidget*,QWidget*)));
    connect(&modifiedTimer, SIGNAL(timeout()), this, SLOT(fileModifiedTimeout()));

#ifdef Q_OS_MAC
    MainWindow* mw = new MainWindow(QString());
    const QMenuBar* mwb = mw->ui->menubar;
    defaultMenuBar = new QMenuBar(0);
    recentFilesMenu = new QMenu("Recent files");
    recentProjectsMenu = new QMenu("Recent projects");
    connect(recentFilesMenu, SIGNAL(triggered(QAction*)), this, SLOT(recentFileMenuAction(QAction*)));
    connect(recentProjectsMenu, SIGNAL(triggered(QAction*)), this, SLOT(recentProjectMenuAction(QAction*)));
    addRecentFile("");
    addRecentProject("");

    QList<QObject*> lst = mwb->children();
    foreach (QObject* mo, lst) {
        if (QMenu* m = qobject_cast<QMenu*>(mo)) {
            if (m->title()=="&File" || m->title()=="Help") {
                QMenu* nm = defaultMenuBar->addMenu(m->title());
                foreach (QAction* a, m->actions()) {
                    if (a->isSeparator()) {
                        nm->addSeparator();
                    } else {
                        if (a->text()=="Recent Files") {
                            nm->addMenu(recentFilesMenu);
                        } else if (a->text()=="Recent Projects") {
                            nm->addMenu(recentProjectsMenu);
                        } else {
                            QAction* na = nm->addAction(a->text());
                            na->setShortcut(a->shortcut());
                            if (a==mw->ui->actionQuit) {
                                connect(na,SIGNAL(triggered()),this,SLOT(quit()));
                            } else if (a==mw->ui->actionNewModel_file || a==mw->ui->actionNew_project) {
                                connect(na,SIGNAL(triggered()),this,SLOT(newProject()));
                            } else if (a==mw->ui->actionOpen) {
                                connect(na,SIGNAL(triggered()),this,SLOT(openFile()));
                            } else if (a==mw->ui->actionHelp) {
                                connect(na,SIGNAL(triggered()),this,SLOT(help()));
                            } else {
                                na->setEnabled(false);
                            }
                        }
                    }
                }
            }
        }
    }
    mainWindows.remove(mw);
    delete mw;
#endif

    checkUpdate();
}

#ifdef Q_OS_MAC

void IDE::recentFileMenuAction(QAction* a) {
    if (a->text()=="Clear Menu") {
        recentFiles.clear();
        recentFilesMenu->clear();
        recentFilesMenu->addSeparator();
        recentFilesMenu->addAction("Clear Menu");
    } else {
        openFile(a->data().toString());
    }
}

void IDE::recentProjectMenuAction(QAction* a) {
    if (a->text()=="Clear Menu") {
        IDE::instance()->recentProjects.clear();
        recentProjectsMenu->clear();
        recentProjectsMenu->addSeparator();
        recentProjectsMenu->addAction("Clear Menu");
    } else {
        openFile(a->data().toString());
    }
}

#endif

void IDE::handleFocusChange(QWidget *old, QWidget *newW)
{
    if (old==NULL && newW!=NULL && !modifiedFiles.empty()) {
        fileModifiedTimeout();
    }
}

void IDE::fileModifiedTimeout(void)
{
    QSet<QString> modCopy = modifiedFiles;
    modifiedFiles.clear();
    for (QSet<QString>::iterator s_it = modCopy.begin(); s_it != modCopy.end(); ++s_it) {
        DMap::iterator it = documents.find(*s_it);
        if (it != documents.end()) {
            QFileInfo fi(*s_it);
            QMessageBox msg;

            if (!fi.exists()) {
                msg.setText("The file "+fi.fileName()+" has been removed or renamed outside MiniZinc IDE.");
                msg.setStandardButtons(QMessageBox::Ok);
            } else {
                msg.setText("The file "+fi.fileName()+" has been modified outside MiniZinc IDE.");

                bool isModified = it.value()->td.isModified();
                if (!isModified) {
                    QFile newFile(fi.absoluteFilePath());
                    if (newFile.open(QFile::ReadOnly | QFile::Text)) {
                        QString newFileContents = newFile.readAll();
                        isModified = (newFileContents != it.value()->td.toPlainText());
                    } else {
                        isModified = true;
                    }
                }

                if (isModified) {
                    if (it.value()->td.isModified()) {
                        msg.setInformativeText("Do you want to reload the file and discard your changes?");
                    } else {
                        msg.setInformativeText("Do you want to reload the file?");
                    }
                    QPushButton* cancelButton = msg.addButton(QMessageBox::Cancel);
                    msg.addButton("Reload", QMessageBox::AcceptRole);
                    msg.exec();
                    if (msg.clickedButton()==cancelButton) {
                        it.value()->td.setModified(true);
                    } else {
                        QFile file(*s_it);
                        if (file.open(QFile::ReadOnly | QFile::Text)) {
                            it.value()->td.setPlainText(file.readAll());
                            it.value()->td.setModified(false);
                        } else {
                            QMessageBox::warning(NULL, "MiniZinc IDE",
                                                 "Could not reload file "+*s_it,
                                                 QMessageBox::Ok);
                            it.value()->td.setModified(true);
                        }
                    }
                }
            }
        }
    }
}

void IDE::fileModified(const QString &f)
{
    modifiedFiles.insert(f);
    if (activeWindow()!=NULL) {
        modifiedTimer.setSingleShot(true);
        modifiedTimer.start(3000);
    }
}

void IDE::addRecentProject(const QString& p) {
    if (p != "") {
        recentProjects.removeAll(p);
        recentProjects.insert(0,p);
        while (recentProjects.size() > 12)
            recentProjects.pop_back();
    }
#ifdef Q_OS_MAC
    recentProjectsMenu->clear();
    for (int i=0; i<recentProjects.size(); i++) {
        QAction* na = recentProjectsMenu->addAction(recentProjects[i]);
        na->setData(recentProjects[i]);
    }
    recentProjectsMenu->addSeparator();
    recentProjectsMenu->addAction("Clear Menu");
#endif
}

void IDE::addRecentFile(const QString& f) {
    if (f != "") {
        recentFiles.removeAll(f);
        recentFiles.insert(0,f);
        while (recentFiles.size() > 12)
            recentFiles.pop_back();
    }
#ifdef Q_OS_MAC
    recentFilesMenu->clear();
    for (int i=0; i<recentFiles.size(); i++) {
        QAction* na = recentFilesMenu->addAction(recentFiles[i]);
        na->setData(recentFiles[i]);
    }
    recentFilesMenu->addSeparator();
    recentFilesMenu->addAction("Clear Menu");
#endif
}

void IDE::newProject()
{
    MainWindow* mw = new MainWindow(QString());
    mw->show();
}

QString IDE::getLastPath(void) {
    QSettings settings;
    settings.beginGroup("Path");
    return settings.value("lastPath","").toString();
}

void IDE::setLastPath(const QString& path) {
    QSettings settings;
    settings.beginGroup("Path");
    settings.setValue("lastPath", path);
    settings.endGroup();
}

void IDE::setEditorFont(QFont font)
{
    QSettings settings;
    settings.beginGroup("MainWindow");
    settings.setValue("editorFont", font.toString());
    settings.endGroup();
    for (QSet<MainWindow*>::iterator it = IDE::instance()->mainWindows.begin();
         it != IDE::instance()->mainWindows.end(); ++it) {
        (*it)->setEditorFont(font);
    }
}

void IDE::openFile(const QString& fileName0)
{
    QString fileName = fileName0;
    if (fileName.isEmpty()) {
        fileName = QFileDialog::getOpenFileName(NULL, tr("Open File"), getLastPath(), "MiniZinc Files (*.mzn *.dzn *.fzn *.mzp)");
        if (!fileName.isNull()) {
            setLastPath(QFileInfo(fileName).absolutePath()+fileDialogSuffix);
        }
    }
    if (!fileName.isEmpty()) {
        MainWindow* mw = new MainWindow(QString());
        if (fileName.endsWith(".mzp")) {
            mw->openProject(fileName);
        } else {
            mw->createEditor(fileName, false, false);
        }
        mw->show();
    }
}

void IDE::help()
{
    QDesktopServices::openUrl(QUrl(QString("http://www.minizinc.org/doc-")+MINIZINC_IDE_VERSION+"/en/minizinc_ide.html"));
}

IDE::~IDE(void) {
    QSettings settings;
    settings.setValue("statistics",stats.toVariantMap());
    settings.beginGroup("Recent");
    settings.setValue("files",recentFiles);
    settings.setValue("projects",recentProjects);
    settings.endGroup();
    settings.beginGroup("ide");
    settings.endGroup();
}

bool IDE::hasFile(const QString& path)
{
    return documents.find(path) != documents.end();
}

QTextDocument* IDE::addDocument(const QString& path, QTextDocument *doc, CodeEditor *ce)
{
    Doc* d = new Doc;
    d->td.setDefaultFont(ce->font());
    d->td.setPlainText(doc->toPlainText());
    d->editors.insert(ce);
    d->large = false;
    documents.insert(path,d);
    fsWatch.addPath(path);
    return &d->td;
}

QPair<QTextDocument*,bool> IDE::loadFile(const QString& path, QWidget* parent)
{
    DMap::iterator it = documents.find(path);
    if (it==documents.end()) {
        QFile file(path);
        if (file.open(QFile::ReadOnly | QFile::Text)) {
            Doc* d = new Doc;
            if ( (path.endsWith(".dzn") || path.endsWith(".fzn")) && file.size() > 5*1024*1024) {
                d->large = true;
            } else {
                d->td.setPlainText(file.readAll());
                d->large = false;
            }
            d->td.setModified(false);
            documents.insert(path,d);
            if (!d->large)
                fsWatch.addPath(path);
            return qMakePair(&d->td,d->large);
        } else {
            QMessageBox::warning(parent, "MiniZinc IDE",
                                 "Could not open file "+path,
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
            QTextStream file_stream(&file);
            file_stream.setCodec("UTF-8");
            it.value()->td.setPlainText(file_stream.readAll());
            it.value()->large = false;
            it.value()->td.setModified(false);
            QSet<CodeEditor*>::iterator ed = it.value()->editors.begin();
            for (; ed != it.value()->editors.end(); ++ed) {
                (*ed)->loadedLargeFile();
            }
            fsWatch.addPath(path);
        } else {
            QMessageBox::warning(parent, "MiniZinc IDE",
                                 "Could not open file "+path,
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
    if (it == documents.end()) {
        qDebug() << "internal error: document " << path << " not found";
    } else {
        QSet<CodeEditor*>& editors = it.value()->editors;
        editors.remove(ce);
        if (editors.empty()) {
            delete it.value();
            documents.remove(path);
            fsWatch.removePath(path);
        }
    }
}

void IDE::renameFile(const QString& oldPath, const QString& newPath)
{
    DMap::iterator it = documents.find(oldPath);
    if (it == documents.end()) {
        qDebug() << "internal error: document " << oldPath << " not found";
    } else {
        Doc* doc = it.value();
        documents.remove(oldPath);
        fsWatch.removePath(oldPath);
        documents.insert(newPath, doc);
        fsWatch.addPath(newPath);
    }
}

void
IDE::versionCheckFinished(void) {
    if (versionCheckReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()==200) {
        QString currentVersion = versionCheckReply->readAll();

        QRegExp versionRegExp("([1-9][0-9]*)\\.([0-9]+)\\.([0-9]+)");

        int curVersionMajor = 0;
        int curVersionMinor = 0;
        int curVersionPatch = 0;
        bool ok = true;
        if (versionRegExp.indexIn(currentVersion) != -1) {
            curVersionMajor = versionRegExp.cap(1).toInt(&ok);
            if (ok)
                curVersionMinor = versionRegExp.cap(2).toInt(&ok);
            if (ok)
                curVersionPatch = versionRegExp.cap(3).toInt(&ok);
        }

        int appVersionMajor = 0;
        int appVersionMinor = 0;
        int appVersionPatch = 0;
        if (ok && versionRegExp.indexIn(applicationVersion()) != -1) {
            appVersionMajor = versionRegExp.cap(1).toInt(&ok);
            if (ok)
                appVersionMinor = versionRegExp.cap(2).toInt(&ok);
            if (ok)
                appVersionPatch = versionRegExp.cap(3).toInt(&ok);
        }

        bool needUpdate = ok && (curVersionMajor > appVersionMajor ||
                                 (curVersionMajor==appVersionMajor &&
                                  (curVersionMinor > appVersionMinor ||
                                   (curVersionMinor==appVersionMinor &&
                                    curVersionPatch > appVersionPatch))));

        if (needUpdate) {
            int button = QMessageBox::information(NULL,"Update available",
                                     "Version "+currentVersion+" of MiniZinc is now available. "
                                     "You are currently using version "+applicationVersion()+
                                     ".\nDo you want to open the MiniZinc web site?",
                                     QMessageBox::Cancel|QMessageBox::Ok,QMessageBox::Ok);
            if (button==QMessageBox::Ok) {
                QDesktopServices::openUrl(QUrl("http://www.minizinc.org/"));
            }
        }
        QSettings settings;
        settings.beginGroup("ide");
        settings.setValue("lastCheck21",QDate::currentDate().toString());
        settings.endGroup();
        stats.resetCounts();
    }
}

QString IDE::appDir(void) const {
#ifdef Q_OS_MAC
    return applicationDirPath()+"/../Resources/";
#else
    return applicationDirPath();
#endif
}

IDE* IDE::instance(void) {
    return static_cast<IDE*>(qApp);
}

MainWindow::MainWindow(const QString& project) :
    ui(new Ui::MainWindow),
    curEditor(NULL),
    curHtmlWindow(-1),
    process(NULL),
    outputProcess(NULL),
    tmpDir(NULL),
    saveBeforeRunning(false),
    project(ui),
    outputBuffer(NULL),
    processRunning(false),
    currentSolverConfig(-1)
{
    init(project);
}

MainWindow::MainWindow(const QStringList& files) :
    ui(new Ui::MainWindow),
    curEditor(NULL),
    curHtmlWindow(-1),
    process(NULL),
    outputProcess(NULL),
    tmpDir(NULL),
    saveBeforeRunning(false),
    project(ui),
    outputBuffer(NULL),
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
    settings.endGroup();

    IDE::instance()->setEditorFont(editorFont);

    settings.beginGroup("minizinc");
    mznDistribPath = settings.value("mznpath","").toString();
    settings.endGroup();
    checkMznPath();

    QVector<SolverConfiguration> builtinConfigs;
    defaultSolverIdx = 0;
    SolverConfiguration::defaultConfigs(solvers, builtinConfigs, defaultSolverIdx);
    for (int i=0; i<builtinConfigs.size(); i++)
        builtinSolverConfigs.push_back(builtinConfigs[i]);
    updateSolverConfigs();
    setCurrentSolverConfig(defaultSolverIdx);
    connect(ui->menuSolver_configurations,SIGNAL(triggered(QAction*)),this,SLOT(on_solverConfigurationSelected(QAction*)));
    connect(solverConfCombo,SIGNAL(currentIndexChanged(int)),this,SLOT(on_conf_solver_conf_currentIndexChanged(int)));
    ui->solverConfNameEdit->hide();
    ui->nameAlreadyUsedLabel->hide();

    connect(QApplication::clipboard(), SIGNAL(dataChanged()), this, SLOT(onClipboardChanged()));

    projectSort = new QSortFilterProxyModel(this);
    projectSort->setDynamicSortFilter(true);
    projectSort->setSourceModel(&project);
    projectSort->setSortRole(Qt::UserRole);
    ui->projectView->setModel(projectSort);
    ui->projectView->sortByColumn(0, Qt::AscendingOrder);
    ui->projectView->setEditTriggers(QAbstractItemView::EditKeyPressed);
    ui->projectExplorerDockWidget->hide();
    ui->conf_dock_widget->hide();
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
        projectRunWith->setEnabled(ui->actionRun->isEnabled() && file.endsWith(".dzn"));
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
        QString fileName = QFileDialog::getOpenFileName(this, tr("Select a data file to open"), getLastPath(), "MiniZinc data files (*.dzn)");
        if (!fileName.isNull())
            fileNames.append(fileName);
    } else {
        fileNames = QFileDialog::getOpenFileNames(this, tr("Select one or more files to open"), getLastPath(), "MiniZinc Files (*.mzn *.dzn)");
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
        fakeCompileAction->setEnabled(true);
        ui->actionCompile->setEnabled(false);
        fakeStopAction->setEnabled(false);
        ui->actionStop->setEnabled(true);
        runButton->removeAction(ui->actionRun);
        runButton->setDefaultAction(ui->actionStop);
        ui->actionSubmit_to_MOOC->setEnabled(false);
    } else {
        bool isMzn = false;
        bool isFzn = false;
        if (curEditor) {
            isMzn = curEditor->filepath=="" || QFileInfo(curEditor->filepath).suffix()=="mzn";
            isFzn = QFileInfo(curEditor->filepath).suffix()=="fzn";
        }
        fakeRunAction->setEnabled(! (isMzn || isFzn));
        ui->actionRun->setEnabled(isMzn || isFzn);
        fakeCompileAction->setEnabled(!isMzn);
        ui->actionCompile->setEnabled(isMzn);
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
    if (project.isProjectFile(index) && ui->conf_dock_widget->isHidden()) {
        on_actionEditSolverConfig_triggered();
    } else {
        QString fileName = project.fileAtIndex(index);
        if (!fileName.isEmpty()) {
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
    if (process) {
        process->terminate();
        delete process;
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

void MainWindow::on_defaultBehaviourButton_toggled(bool checked)
{
    ui->defaultBehaviourFrame->setEnabled(checked);
    ui->userBehaviourFrame->setEnabled(!checked);
}

void MainWindow::createEditor(const QString& path, bool openAsModified, bool isNewFile, bool readOnly, bool focus) {
    QTextDocument* doc = NULL;
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
        fileName = QFileDialog::getOpenFileName(this, tr("Open File"), getLastPath(), "MiniZinc Files (*.mzn *.dzn *.fzn *.mzp)");
        if (!fileName.isNull()) {
            setLastPath(QFileInfo(fileName).absolutePath()+fileDialogSuffix);
        }
    }

    if (!fileName.isEmpty()) {
        if (fileName.endsWith(".mzp")) {
            openProject(fileName);
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
    ui->tabWidget->removeTab(tab);
    if (!ce->filepath.isEmpty())
        IDE::instance()->removeEditor(ce->filepath,ce);
    delete ce;
}

void MainWindow::closeEvent(QCloseEvent* e) {
    // make sure any modifications in solver configurations are saved
    on_conf_solver_conf_currentIndexChanged(ui->conf_solver_conf->currentIndex());

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
                   this, 0);
        process->terminate();
        delete process;
        process = NULL;
    }
    for (int i=0; i<ui->tabWidget->count(); i++) {
        CodeEditor* ce = static_cast<CodeEditor*>(ui->tabWidget->widget(i));
        ce->setDocument(NULL);
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
    }
    if (tab==-1) {
        curEditor = NULL;
        ui->actionClose->setEnabled(false);
    } else {
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
        connect(curEditor->document(), SIGNAL(undoAvailable(bool)),
                ui->actionUndo, SLOT(setEnabled(bool)));
        connect(curEditor->document(), SIGNAL(redoAvailable(bool)),
                ui->actionRedo, SLOT(setEnabled(bool)));
        connect(curEditor, SIGNAL(cursorPositionChanged()), this, SLOT(editor_cursor_position_changed()));
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
            if (haveChecker && ui->conf_check_solutions->isChecked()) {
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

QStringList parseArgList(const QString& s) {
    QStringList ret;
    bool hadEscape = false;
    bool inSingleQuote = false;
    bool inDoubleQuote = false;
    QString currentArg;
    foreach (const QChar c, s) {
        if (hadEscape) {
            currentArg += c;
            hadEscape = false;
        } else {
            if (c=='\\') {
                hadEscape = true;
            } else if (c=='"') {
                if (inDoubleQuote) {
                    inDoubleQuote=false;
                    ret.push_back(currentArg);
                    currentArg = "";
                } else if (inSingleQuote) {
                    currentArg += c;
                } else {
                    inDoubleQuote = true;
                }
            } else if (c=='\'') {
                if (inSingleQuote) {
                    inSingleQuote=false;
                    ret.push_back(currentArg);
                    currentArg = "";
                } else if (inDoubleQuote) {
                    currentArg += c;
                } else {
                    inSingleQuote = true;
                }
            } else if (!inSingleQuote && !inDoubleQuote && c==' ') {
                if (currentArg.size() > 0) {
                    ret.push_back(currentArg);
                    currentArg = "";
                }
            } else {
                currentArg += c;
            }
        }
    }
    if (currentArg.size() > 0) {
        ret.push_back(currentArg);
    }
    return ret;
}

QStringList MainWindow::parseConf(const ConfMode& confMode, const QString& modelFile, bool isOptimisation)
{
    Solver& currentSolver = *getCurrentSolver();
    bool haveThreads = currentSolver.stdFlags.contains("-p");
    bool haveSeed = currentSolver.stdFlags.contains("-r");
    bool haveStats = currentSolver.stdFlags.contains("-s");
    bool haveAllSol = currentSolver.stdFlags.contains("-a");
    bool haveNSol = currentSolver.stdFlags.contains("-n");
    bool haveFreeSearch = currentSolver.stdFlags.contains("-f");
    bool haveSolverVerbose = currentSolver.stdFlags.contains("-v");
    bool haveOutputHtml = currentSolver.stdFlags.contains("--output-html");
    bool haveNeedsPaths = currentSolver.needsPathsFile;

    bool isMiniZinc = (confMode!=CONF_RUN && !currentSolver.supportsMzn) || currentSolver.executable.isEmpty() || !currentSolver.supportsFzn;
    bool haveCompilerVerbose =  isMiniZinc || (currentSolver.supportsMzn && currentSolver.stdFlags.contains("-v"));
    bool haveCompilerStats =  isMiniZinc || currentSolver.stdFlags.contains("-s");
    bool haveCompilerOpt[6];
    haveCompilerOpt[0] =  isMiniZinc || currentSolver.stdFlags.contains("-O0");
    haveCompilerOpt[1] =  isMiniZinc || currentSolver.stdFlags.contains("-O1");
    haveCompilerOpt[2] =  isMiniZinc || currentSolver.stdFlags.contains("-O2");
    haveCompilerOpt[3] =  isMiniZinc || currentSolver.stdFlags.contains("-O3");
    haveCompilerOpt[4] =  isMiniZinc || currentSolver.stdFlags.contains("-O4");
    haveCompilerOpt[5] =  isMiniZinc || currentSolver.stdFlags.contains("-O5");
    bool haveCompilerCheck = isMiniZinc;

    QStringList ret;
    if (confMode==CONF_RUN && haveOutputHtml) {
        ret << "--output-html";
    }
    if (confMode==CONF_COMPILE && haveNeedsPaths) {
        ret << "--paths";
        ret << currentPathsTarget;
    }
    if (confMode==CONF_COMPILE || confMode==CONF_CHECKARGS) {
        for (auto& a : currentSolver.defaultFlags) {
            ret << a;
        }
    }
    if (confMode==CONF_COMPILE) {
        int optLevel = ui->conf_optlevel->currentIndex();
        if (optLevel < 6 && haveCompilerOpt[optLevel])
            ret << "-O"+QString().number(optLevel);
    }
    if (confMode==CONF_COMPILE && ui->conf_verbose->isChecked() && haveCompilerVerbose)
        ret << (isMiniZinc ? "--verbose-compilation" : "-v");
    if (confMode==CONF_COMPILE && ui->conf_flatten_stats->isChecked() && haveCompilerStats)
        ret << "-s";
    if ( (confMode==CONF_COMPILE || confMode==CONF_CHECKARGS) && !ui->conf_cmd_params->text().isEmpty())
        ret << "-D" << ui->conf_cmd_params->text();
    if (confMode==CONF_COMPILE && ui->conf_check_solutions->isChecked() && haveCompilerCheck) {
        if (modelFile.endsWith(".mzn")) {
            QString checkFile = modelFile;
            checkFile.replace(checkFile.length()-1,1,"c");
            if (project.containsFile(checkFile))
                ret << checkFile;
            else if (project.containsFile(checkFile+".mzn"))
                ret << checkFile+".mzn";
        }
    }

    if ((confMode==CONF_COMPILE || confMode==CONF_CHECKARGS) && !ui->conf_mzn2fzn_params->text().isEmpty()) {
        QStringList compilerArgs = parseArgList(ui->conf_mzn2fzn_params->text());
        ret << compilerArgs;
    }

    if (confMode==CONF_RUN) {
        if (ui->defaultBehaviourButton->isChecked()) {
            if (isOptimisation && haveAllSol)
                ret << "-a";
        } else {
            if (isOptimisation) {
                if (ui->conf_printall->isChecked() && haveAllSol)
                    ret << "-a";
            } else {
                if (ui->conf_nsol->value() == 0 && haveAllSol)
                    ret << "-a";
                else if (ui->conf_nsol->value() > 1 && haveNSol)
                    ret << "-n" << QString::number(ui->conf_nsol->value());
            }
        }
    }

    if (confMode==CONF_RUN && ui->conf_stats->isChecked() && haveStats)
        ret << "-s";
    if (confMode==CONF_RUN && ui->conf_solver_free->isChecked() && haveFreeSearch)
        ret << "-f";
    if (confMode==CONF_RUN && ui->conf_nthreads->value() > 1 && haveThreads)
        ret << "-p" << QString::number(ui->conf_nthreads->value());
    if (confMode==CONF_RUN && ui->conf_have_seed->isChecked() && haveSeed)
        ret << "-r" << ui->conf_seed->text();
    if (confMode==CONF_RUN && ui->conf_solver_verbose->isChecked() && haveSolverVerbose)
        ret << (isMiniZinc ? "--verbose-solving" : "-v");
    if (confMode==CONF_RUN && !ui->conf_solverFlags->text().isEmpty()) {
        QStringList solverArgs = parseArgList(ui->conf_solverFlags->text());
        ret << solverArgs;
    }
    if (confMode==CONF_RUN) {
        for (auto& ef : extraSolverFlags) {
            if (ef.first.t==SolverFlag::T_SOLVER) {

            } else {
                switch (ef.first.t) {
                case SolverFlag::T_BOOL:
                    if (static_cast<QCheckBox*>(ef.second)->isChecked()) {
                        ret << ef.first.name;
                    }
                    break;
                case SolverFlag::T_OPT:
                case SolverFlag::T_SOLVER:
                    ret << ef.first.name << static_cast<QComboBox*>(ef.second)->currentText();
                    break;
                case SolverFlag::T_INT:
                case SolverFlag::T_FLOAT:
                case SolverFlag::T_STRING:
                {
                    QString s = static_cast<QLineEdit*>(ef.second)->text();
                    if (!s.isEmpty() && s!=ef.first.def) {
                        ret << ef.first.name << s;
                    }
                }
                    break;
                }

            }
        }
    }
    if (confMode==CONF_COMPILE || confMode==CONF_CHECKARGS) {
        ret << "--solver" << currentSolver.id+(currentSolver.version.startsWith("<unknown") ? "" : ("@"+currentSolver.version));
    }
    return ret;
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

void MainWindow::checkArgsOutput()
{
    QString l = process->readAll();
    compileErrors += l;
}

QString MainWindow::getMznDistribPath(void) const {
    return mznDistribPath;
}

void MainWindow::checkArgsFinished(int exitcode, QProcess::ExitStatus exitstatus)
{
    if (processWasStopped || exitstatus==QProcess::CrashExit)
        return;
    QString additionalCmdlineParams;
    QStringList additionalDataFiles;
    if (process) {
        checkArgsOutput();
    }
    if (exitcode==0) {
        if (!compileErrors.isEmpty()) {
            QJsonDocument jdoc = QJsonDocument::fromJson(compileErrors.toUtf8());
            compileErrors = "";
            if (jdoc.isObject() && jdoc.object()["input"].isObject() && jdoc.object()["method"].isString()) {
                isOptimisation = (jdoc.object()["method"].toString() != "sat");
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
                                    QMessageBox::critical(this, "Undefined parameter","The parameter `"+undefinedArgs[i]+"' is undefined.");
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
                qDebug() << compileErrors;
                QMessageBox::critical(this, "Internal error", "Could not determine model parameters");
                procFinished(0);
                return;
            }
        }
    }

    for (QString dzn: currentAdditionalDataFiles)
        additionalDataFiles.append(dzn);
    currentAdditionalDataFiles.clear();
    runTimeout = ui->conf_timeLimit->value();
    compileAndRun(curModelFilepath, additionalCmdlineParams, additionalDataFiles);
}

void MainWindow::checkArgs(QString filepath)
{
    if (minizinc_executable=="") {
        int ret = QMessageBox::warning(this,"MiniZinc IDE","Could not find the minizinc executable.\nDo you want to open the solver settings dialog?",
                                       QMessageBox::Ok | QMessageBox::Cancel);
        if (ret == QMessageBox::Ok)
            on_actionManage_solvers_triggered();
        return;
    }
    processName = minizinc_executable;
    curModelFilepath = filepath;
    processWasStopped = false;
    compileErrors = "";
    if (compileOnly && filepath.endsWith(".mzc.mzn")) {
        // We are compiling a solution checker
        process = NULL;
        checkArgsFinished(0, QProcess::NormalExit);
    } else {
        process = new MznProcess(this);
        process->setWorkingDirectory(QFileInfo(filepath).absolutePath());
        process->setProcessChannelMode(QProcess::MergedChannels);
        connect(process, SIGNAL(readyRead()), this, SLOT(checkArgsOutput()));
        connect(process, SIGNAL(finished(int,QProcess::ExitStatus)), this, SLOT(checkArgsFinished(int,QProcess::ExitStatus)));
        connect(process, SIGNAL(error(QProcess::ProcessError)),
                this, SLOT(checkArgsError(QProcess::ProcessError)));

        QStringList args = parseConf(CONF_CHECKARGS, "", false);
        args << "-c" << "--model-interface-only";
        for (QString dzn: currentAdditionalDataFiles)
            args << dzn;
        args << filepath;
        elapsedTime.start();
        process->start(minizinc_executable,args,getMznDistribPath());
    }
}

void MainWindow::on_actionRun_triggered()
{
    compileOnly = false;
    curHtmlWindow = -1;
    compileOrRun();
}

void MainWindow::compileOrRun()
{
    if (minizinc_executable=="") {
        int ret = QMessageBox::warning(this,"MiniZinc IDE","Could not find the minizinc executable.\nDo you want to open the solver settings dialog?",
                                       QMessageBox::Ok | QMessageBox::Cancel);
        if (ret == QMessageBox::Ok)
            on_actionManage_solvers_triggered();
        return;
    }
    if (curEditor) {
        QString filepath;
        bool docIsModified;
        if (curEditor->filepath!="") {
            filepath = curEditor->filepath;
            docIsModified = curEditor->document()->isModified();
        } else {
            QTemporaryDir* modelTmpDir = new QTemporaryDir;
            if (!modelTmpDir->isValid()) {
                QMessageBox::critical(this, "MiniZinc IDE", "Could not create temporary directory for compilation.");
            } else {
                cleanupTmpDirs.append(modelTmpDir);
                docIsModified = false;
                filepath = modelTmpDir->path()+"/untitled_model.mzn";
                QFile modelFile(filepath);
                if (modelFile.open(QIODevice::ReadWrite)) {
                    QTextStream ts(&modelFile);
                    ts << curEditor->document()->toPlainText();
                    modelFile.close();
                } else {
                    QMessageBox::critical(this, "MiniZinc IDE", "Could not write temporary model file.");
                    filepath = "";
                }
            }
        }
        if (filepath != "") {
            if (docIsModified) {
                if (!saveBeforeRunning) {
                    QMessageBox msgBox;
                    msgBox.setText("The model has been modified. You have to save it before running.");
                    msgBox.setInformativeText("Do you want to save it now and then run?");
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
            if (curEditor->filepath!="" && curEditor->document()->isModified())
                return;
            if (ui->autoclear_output->isChecked()) {
                on_actionClear_output_triggered();
            }
            updateUiProcessRunning(true);
            if (!compileOnly) {
                on_actionSplit_triggered();
                IDE::instance()->stats.modelsRun++;
                if (filepath.endsWith(".fzn")) {
                    processWasStopped = false;
                    currentFznTarget = filepath;
                    runSolns2Out = false;
                    runTimeout = ui->conf_timeLimit->value();
                    if (runTimeout > 0) {
                        solverTimeout->start(runTimeout*1000);
                    }
                    isOptimisation = true;
                    solutionCount = 0;
                    solutionLimit = ui->conf_compressSolutionLimit->value();
                    hiddenSolutions.clear();
                    inJSONHandler = false;
                    curJSONHandler = 0;
                    JSONOutput.clear();
                    if (curHtmlWindow >= 0) {
                        curHtmlWindow = -1;
                    }
                    hadNonJSONOutput = false;

                    if (!currentFznTarget.isEmpty()) {
                        QFile fznFile(currentFznTarget);
                        if (fznFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                            int seekSize = strlen("satisfy;\n\n");
                            if (fznFile.size() >= seekSize) {
                                fznFile.seek(fznFile.size()-seekSize);
                                QString line = fznFile.readLine();
                                if (line.contains("satisfy;"))
                                    isOptimisation = false;
                            }
                        }
                    }
                    elapsedTime.start();
                    runCompiledFzn(0,QProcess::NormalExit);
                    return;
                }
            }
            checkArgs(filepath);
        }
    }
}

QString MainWindow::setElapsedTime()
{
    qint64 elapsed_t = elapsedTime.elapsed();
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
    if (ui->conf_timeLimit->value() > 0) {
        timeLimit += " / ";
        int tl_hours = ui->conf_timeLimit->value() / 3600;
        int tl_minutes = (ui->conf_timeLimit->value() % 3600) / 60;
        int tl_seconds = ui->conf_timeLimit->value() % 60;
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
    for (int i=time; i--;) txt+=".";
    ui->statusbar->showMessage(txt);
    time = (time+1) % 5;
    setElapsedTime();
}

void MainWindow::readOutput()
{
    MznProcess* readProc = (outputProcess==NULL ? process : outputProcess);
    if (readProc != NULL) {
        readProc->setReadChannel(QProcess::StandardOutput);
        while (readProc->canReadLine()) {
            QString l = readProc->readLine();
            if (inJSONHandler) {
                l = l.trimmed();
                if (l.startsWith("%%%mzn-json-time")) {
                    JSONOutput[curJSONHandler].insert(2, "[");
                    JSONOutput[curJSONHandler].append(","+ QString().number(elapsedTime.elapsed()) +"]\n");
                } else if (l.startsWith("%%%mzn-json-end")) {
                    curJSONHandler++;
                    inJSONHandler = false;
                } else if (l.startsWith("%%%mzn-json-init-end")) {
                    curJSONHandler++;
                    inJSONHandler = false;
                    openJSONViewer();
                    JSONOutput.clear();
                    curJSONHandler = 0;
                    if (hadNonJSONOutput)
                        addOutput(l,false);
                } else {
                    JSONOutput[curJSONHandler].append(l);
                }
            } else {
                QRegExp pattern("^(?:%%%(top|bottom))?%%%mzn-json(-init)?:(.*)");
                if (pattern.exactMatch(l.trimmed())) {
                    inJSONHandler = true;
                    isJSONinitHandler = (pattern.capturedTexts()[2]=="-init");
                    QStringList sl;
                    sl.append(pattern.capturedTexts()[3]);
                    if (pattern.capturedTexts()[1].isEmpty()) {
                        sl.append("top");
                    } else {
                        sl.append(pattern.capturedTexts()[1]);
                    }
                    JSONOutput.append(sl);
                } else if (l.trimmed().startsWith("%%%mzn-html-start")) {
                    inHTMLHandler = true;
                } else if (l.trimmed().startsWith("%%%mzn-html-end")) {
                    addOutput(htmlBuffer.join(""), true);
                    htmlBuffer.clear();
                    inHTMLHandler = false;
                } else if (l.trimmed().startsWith("%%%mzn-progress")) {
                    float value = l.split(" ")[1].toFloat();
                    progressBar->setHidden(false);
                    progressBar->setValue(std::lround(value));
                } else {
                    if (l.trimmed() == "----------") {
                        solutionCount++;
                        if ( (solutionLimit != 0 && solutionCount > solutionLimit) || !hiddenSolutions.isEmpty()) {
                            if (hiddenSolutions.isEmpty()) {
                                solutionCount = 0;
                                if (!curJSONHandler || hadNonJSONOutput)
                                    addOutput(l,false);
                            }
                            else
                                hiddenSolutions.back() += l;
                            hiddenSolutions.append("");
                        }
                    }
                    if (curJSONHandler > 0 && l.trimmed() == "----------") {
                        openJSONViewer();
                        JSONOutput.clear();
                        curJSONHandler = 0;
                        if (hadNonJSONOutput)
                            addOutput(l,false);
                    } else if (curHtmlWindow>=0 && l.trimmed() == "==========") {
                        finishJSONViewer();
                        if (hadNonJSONOutput)
                            addOutput(l,false);
                    } else {
                        if (outputBuffer)
                            (*outputBuffer) << l;
                        if (!hiddenSolutions.isEmpty()) {
                            if (l.trimmed() != "----------") {
                                hiddenSolutions.back() += l;
                            }
                            if (solutionLimit != 0 && solutionCount == solutionLimit) {
                                addOutput("<div class='mznnotice'>[ "+QString().number(solutionLimit-1)+" more solutions ]</div>");
                                for (int i=std::max(0,hiddenSolutions.size()-2); i<hiddenSolutions.size(); i++) {
                                    addOutput(hiddenSolutions[i], false);
                                }
                                solutionCount = 0;
                                solutionLimit *= 2;
                            }
                        } else if(inHTMLHandler){
                            htmlBuffer << l;
                        } else {
                            addOutput(l, false);
                        }
                        if (!hiddenSolutions.isEmpty() && l.trimmed() == "==========") {
                            if (solutionCount!=solutionLimit && solutionCount > 1) {
                                addOutput("<div class='mznnotice'>[ "+QString().number(solutionCount-1)+" more solutions ]</div>");
                            }
                            for (int i=std::max(0,hiddenSolutions.size()-2); i<hiddenSolutions.size(); i++) {
                                addOutput(hiddenSolutions[i], false);
                            }
                            hiddenSolutions.clear();
                        }
                        hadNonJSONOutput = true;
                    }
                }
            }
        }
        // Reset read channel so readyRead() signal is triggered correctly
        readProc->setReadChannel(QProcess::StandardOutput);
    }

    if (process != NULL) {
        process->setReadChannel(QProcess::StandardError);
        for (;;) {
            QString l;
            if (process->canReadLine()) {
                l = process->readLine();
            } else if (process->state()==QProcess::NotRunning) {
                if (process->atEnd())
                    break;
                l = process->readAll()+"\n";
            } else {
                break;
            }
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
        // Reset read channel so readyRead() signal is triggered correctly
        process->setReadChannel(QProcess::StandardOutput);
    }

    if (outputProcess != NULL) {
        outputProcess->setReadChannel(QProcess::StandardError);
        for (;;) {
            QString l;
            if (outputProcess->canReadLine()) {
                l = outputProcess->readLine();
            } else if (outputProcess->state()==QProcess::NotRunning) {
                if (outputProcess->atEnd())
                    break;
                l = outputProcess->readAll()+"\n";
            } else {
                break;
            }
            addOutput(l,false);
        }
        // Reset read channel so readyRead() signal is triggered correctly
        outputProcess->setReadChannel(QProcess::StandardOutput);
    }
}

void MainWindow::openJSONViewer(void)
{
    if (curHtmlWindow==-1) {
        QVector<VisWindowSpec> specs;
        for (int i=0; i<JSONOutput.size(); i++) {
            QString url = JSONOutput[i].first();
            Qt::DockWidgetArea area = Qt::TopDockWidgetArea;
            if (JSONOutput[i][1]=="top") {
                area = Qt::TopDockWidgetArea;
            } else if (JSONOutput[i][1]=="bottom") {
                area = Qt::BottomDockWidgetArea;
            }
            url.remove(QRegExp("[\\n\\t\\r]"));
            specs.append(VisWindowSpec(url,area));
        }
        QFileInfo htmlWindowTitleFile(curFilePath);
        HTMLWindow* htmlWindow = new HTMLWindow(specs, this, htmlWindowTitleFile.fileName());
        curHtmlWindow = htmlWindow->getId();
        htmlWindow->init();
        connect(htmlWindow, SIGNAL(closeWindow(int)), this, SLOT(closeHTMLWindow(int)));
        htmlWindow->show();
    }
    for (int i=0; i<JSONOutput.size(); i++) {
        JSONOutput[i].pop_front();
        JSONOutput[i].pop_front();
        if (htmlWindows[curHtmlWindow]) {
            if (isJSONinitHandler) {
                htmlWindows[curHtmlWindow]->initJSON(i, JSONOutput[i].join(' '));
            } else {
                htmlWindows[curHtmlWindow]->addSolution(i, JSONOutput[i].join(' '));
            }
        }
    }
}

void MainWindow::finishJSONViewer(void)
{
    if (curHtmlWindow >= 0 && htmlWindows[curHtmlWindow]) {
        htmlWindows[curHtmlWindow]->finish(elapsedTime.elapsed());
    }
}

SolverConfiguration* MainWindow::getCurrentSolverConfig(void) {
    int idx = ui->conf_solver_conf->currentIndex();
    if (projectSolverConfigs.size()!=0 && idx > projectSolverConfigs.size())
        idx--;
    SolverConfiguration& conf = idx < projectSolverConfigs.size() ? projectSolverConfigs[idx] : builtinSolverConfigs[idx-projectSolverConfigs.size()];
    return &conf;
}

Solver* MainWindow::getCurrentSolver(void) {
    SolverConfiguration* conf = getCurrentSolverConfig();
    Solver* s = NULL;
    for (int i=0; i<solvers.size(); i++) {
        if (conf->solverId==solvers[i].id) {
            s = &solvers[i];
            if (conf->solverVersion==solvers[i].version)
                break;
        }
    }
    return s;
}

void MainWindow::compileAndRun(const QString& modelPath, const QString& additionalCmdlineParams, const QStringList& additionalDataFiles)
{
    Solver& currentSolver = *getCurrentSolver();
    progressBar->setHidden(true);
    process = new MznProcess(this);
    processName = minizinc_executable;

    bool standalone = false;
    if (!compileOnly) {
        // Check if we need to run a stand-alone solver (no mzn2fzn or solns2out)
        if (currentSolver.supportsMzn || !currentSolver.supportsFzn) {
            standalone = true;
        }
    }

    curFilePath = modelPath;
    processWasStopped = false;
    runSolns2Out = currentSolver.needsSolns2Out;
    process->setWorkingDirectory(QFileInfo(modelPath).absolutePath());
    connect(process, SIGNAL(readyRead()), this, SLOT(readOutput()));
    connect(process, SIGNAL(readyReadStandardError()), this, SLOT(readOutput()));
    if (compileOnly) {
        connect(process, SIGNAL(finished(int)), this, SLOT(openCompiledFzn(int)));
    } else if (standalone) {
        connect(process, SIGNAL(finished(int)), this, SLOT(procFinished(int)));
    } else {
        connect(process, SIGNAL(finished(int,QProcess::ExitStatus)), this, SLOT(runCompiledFzn(int,QProcess::ExitStatus)));
    }
    connect(process, SIGNAL(error(QProcess::ProcessError)),
            this, SLOT(procError(QProcess::ProcessError)));

    QStringList args = parseConf(CONF_COMPILE, modelPath, false);
    if (!additionalCmdlineParams.isEmpty()) {
        args << "-D" << additionalCmdlineParams;
    }
    if (!additionalDataFiles.isEmpty()) {
        for (auto df: additionalDataFiles)
            args << df;
    }

    solutionCount = 0;
    solutionLimit = ui->conf_compressSolutionLimit->value();
    hiddenSolutions.clear();
    inJSONHandler = false;
    curJSONHandler = 0;
    JSONOutput.clear();
    hadNonJSONOutput = false;

    if (standalone) {
        QStringList runArgs = parseConf(CONF_RUN,modelPath,isOptimisation);
        args << runArgs;
    } else {
        if (modelPath.endsWith(".mzc.mzn")) {
            args << "--compile-solution-checker";
            currentFznTarget = modelPath.mid(0,modelPath.size()-4);
        } else {
            args << "-c";
        }
    }
    tmpDir = new QTemporaryDir;
    if (!tmpDir->isValid()) {
        QMessageBox::critical(this, "MiniZinc IDE", "Could not create temporary directory for compilation.");
        procFinished(0);
    } else {
        QFileInfo fi(modelPath);
        if (!standalone && !modelPath.endsWith(".mzc.mzn")) {
            currentFznTarget = tmpDir->path()+"/"+fi.baseName()+".fzn";
            args << "-o" << currentFznTarget;
            args << "--output-ozn-to-file" << tmpDir->path()+"/"+fi.baseName()+".ozn";
            if(currentSolver.needsPathsFile) {
                currentPathsTarget = tmpDir->path()+"/"+fi.baseName()+".paths";
                args << "--output-paths-to-file";
                args << currentPathsTarget;
            }
        }
        args << modelPath;
        QString compiling = (standalone ? "Running " : "Compiling ") + fi.fileName();
        if (!additionalDataFiles.isEmpty()) {
            compiling += ", with additional data ";
            for (auto df: additionalDataFiles) {
                QFileInfo fi(df);
                compiling += fi.fileName()+" ";
            }
        }
        if (!additionalCmdlineParams.isEmpty()) {
            compiling += ", additional arguments " + additionalCmdlineParams;
        }
        addOutput("<div class='mznnotice'>"+compiling+"</div>");
        time = 0;
        timer->start(500);
        elapsedTime.start();
        if (runTimeout != 0) {
            if (standalone) {
                if (currentSolver.executable.isEmpty()) {
                    args.push_front(QString().number(runTimeout*1000));
                    args.push_front("--time-limit");
                    solverTimeout->start((runTimeout+1)*1000);
                } else if (currentSolver.stdFlags.contains("-t")) {
                    args.push_front(QString().number(runTimeout*1000));
                    args.push_front("-t");
                    solverTimeout->start((runTimeout+1)*1000);
                } else {
                    solverTimeout->start(runTimeout*1000);
                }
            } else {
                args.push_front(QString().number(runTimeout*1000));
                args.push_front("--time-limit");
                solverTimeout->start((runTimeout+1)*1000);
            }
        }

        process->start(processName,args,getMznDistribPath());
    }
}

bool MainWindow::runWithOutput(const QString &modelFile, const QString &dataFile, int timeout, QTextStream &outstream)
{
    bool foundModel = false;
    bool foundData = false;
    QString modelFilePath;
    QString dataFilePath;

    QString dataFileRelative = dataFile;
    if (dataFileRelative.startsWith("."))
        dataFileRelative.remove(0,1);

    const QStringList& files = project.files();
    for (int i=0; i<files.size(); i++) {
        QFileInfo fi(files[i]);
        if (fi.fileName() == modelFile) {
            foundModel = true;
            modelFilePath = fi.absoluteFilePath();
        } else if (fi.absoluteFilePath().endsWith(dataFileRelative)) {
            foundData = true;
            dataFilePath = fi.absoluteFilePath();
        }
    }

    if (!foundModel || !foundData) {
        return false;
    }

    outputBuffer = &outstream;
    compileOnly = false;
    runTimeout = timeout;
    curHtmlWindow = -1;
    updateUiProcessRunning(true);
    on_actionSplit_triggered();
    QStringList dataFiles;
    dataFiles.push_back(dataFilePath);
    compileAndRun(modelFilePath,"",dataFiles);
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
            if (outputProcess) {
                outputProcess->disconnect();
                outputProcess->terminate();
                delete outputProcess;
                outputProcess = NULL;
            }
            on_actionStop_triggered();
            compileAndRun(htmlWindowModels[htmlWindowIdentifier],"",dataFiles);
        } else {
            QMessageBox::critical(this, "MiniZinc IDE", "Could not write temporary model file.");
        }
    }

}

QString MainWindow::currentSolver() const
{
    return ui->conf_solver->text();
}

QString MainWindow::currentSolverConfigName(void) {
    SolverConfiguration* sc = getCurrentSolverConfig();
    return sc ? sc->name : "None";
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
    delete htmlWindows[identifier];
    htmlWindows[identifier] = NULL;
}

void MainWindow::selectJSONSolution(HTMLPage* source, int n)
{
    if (curHtmlWindow >= 0 && htmlWindows[curHtmlWindow]) {
        htmlWindows[curHtmlWindow]->selectSolution(source,n);
    }
}

void MainWindow::outputProcFinished(int exitCode, bool showTime) {
    if (exitCode != 0) {
        addOutput("<div style='color:red;'>Solutions processor finished with non-zero exit code "+QString().number(exitCode)+"</div>");
    }
    readOutput();
    updateUiProcessRunning(false);
    timer->stop();
    QString elapsedTime = setElapsedTime();
    ui->statusbar->clearMessage();
    progressBar->reset();
    process = NULL;
    outputProcess = NULL;
    finishJSONViewer();
    inJSONHandler = false;
    JSONOutput.clear();
    if (!hiddenSolutions.isEmpty()) {
        if (solutionLimit != 0 && solutionCount!=solutionLimit && solutionCount > 1) {
            addOutput("<div class='mznnotice'>[ "+QString().number(solutionCount-1)+" more solutions ]</div>");
        }
        for (int i=std::max(0,hiddenSolutions.size()-2); i<hiddenSolutions.size(); i++) {
            addOutput(hiddenSolutions[i], false);
        }
    }

    if (showTime) {
        addOutput("<div class='mznnotice'>Finished in "+elapsedTime+"</div>");
    }
    delete tmpDir;
    tmpDir = NULL;
    outputBuffer = NULL;
    compileErrors = "";
    emit(finished());
}

void MainWindow::procFinished(int exitCode, bool showTime) {
    if (exitCode != 0) {
        addOutput("<div style='color:red;'>Process finished with non-zero exit code "+QString().number(exitCode)+"</div>");
    }
    if (outputProcess && outputProcess->state()!=QProcess::NotRunning) {
        connect(outputProcess, SIGNAL(finished(int)), this, SLOT(outputProcFinished(int)));
        outputProcess->closeWriteChannel();
        inHTMLHandler = false;
        return;
    }
    updateUiProcessRunning(false);
    timer->stop();
    solverTimeout->stop();
    QString elapsedTime = setElapsedTime();
    ui->statusbar->clearMessage();
    process = NULL;
    outputProcess = NULL;
    finishJSONViewer();
    inJSONHandler = false;
    JSONOutput.clear();
    if (showTime) {
        addOutput("<div class='mznnotice'>Finished in "+elapsedTime+"</div>");
    }
    delete tmpDir;
    tmpDir = NULL;
    outputBuffer = NULL;
    compileErrors = "";
    emit(finished());
}

void MainWindow::procError(QProcess::ProcessError e) {
    if (!compileErrors.isEmpty()) {
        addOutput(compileErrors,false);
    }
    procFinished(0);
    if (e==QProcess::FailedToStart) {
        QMessageBox::critical(this, "MiniZinc IDE", "Failed to start '"+processName+"'. Check your path settings.");
    } else {
        QMessageBox::critical(this, "MiniZinc IDE", "Unknown error while executing the MiniZinc interpreter `"+processName+"': error code "+QString().number(e));
    }
}

void MainWindow::checkArgsError(QProcess::ProcessError e) {
    checkArgsOutput();
    if (!compileErrors.isEmpty()) {
        addOutput(compileErrors,false);
    }
    procFinished(0);
    if (e==QProcess::FailedToStart) {
        QMessageBox::critical(this, "MiniZinc IDE", "Failed to start '"+processName+"'. Check your path settings.");
    } else {
        QMessageBox::critical(this, "MiniZinc IDE", "Unknown error while executing the MiniZinc interpreter `"+processName+"': error code "+QString().number(e));
    }
}

void MainWindow::outputProcError(QProcess::ProcessError e) {
    procFinished(0);
    if (e==QProcess::FailedToStart) {
        QMessageBox::critical(this, "MiniZinc IDE", "Failed to start 'solns2out'. Check your path settings.");
    } else {
        QMessageBox::critical(this, "MiniZinc IDE", "Unknown error while executing the MiniZinc solution processor.");
    }
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
        else if (dialogPath.endsWith(".dzn"))
            selectedFilter = "MiniZinc data (*.dzn)";
        else if (dialogPath.endsWith(".fzn"))
            selectedFilter = "FlatZinc (*.fzn)";
        else if (dialogPath.endsWith(".mzc"))
            selectedFilter = "MiniZinc solution checker (*.mzc)";
        filepath = QFileDialog::getSaveFileName(this,"Save file",dialogPath,"MiniZinc model (*.mzn);;MiniZinc data (*.dzn);;MiniZinc solution checker (*.mzc);;FlatZinc (*.fzn);;Other (*)",&selectedFilter);
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
    if (process) {
        disconnect(process, SIGNAL(error(QProcess::ProcessError)),
                   this, 0);
        disconnect(process, SIGNAL(finished(int)), this, 0);
        processWasStopped = true;

        process->terminate();
        delete process;
        process = NULL;
        addOutput("<div class='mznnotice'>Stopped.</div>");
        procFinished(0);
    }
}

void MainWindow::openCompiledFzn(int exitcode)
{
    if (processWasStopped)
        return;
    if (exitcode==0) {
        QFile file(currentFznTarget);
        int fsize = file.size() / (1024*1024);
        if (file.size() > 10*1024*1024) {
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
                    QFile newfile(currentFznTarget);
                    newfile.copy(savepath);
                }
                } while (!success);
                procFinished(exitcode);
                return;
            }
            case QMessageBox::Discard:
                procFinished(exitcode);
                return;
            default:
                break;
            }
        }
        openFile(currentFznTarget, !currentFznTarget.endsWith(".mzc"));
    }
    procFinished(exitcode);
}

void MainWindow::runCompiledFzn(int exitcode, QProcess::ExitStatus exitstatus)
{
    if (processWasStopped)
        return;
    if (exitcode==0 && exitstatus==QProcess::NormalExit) {
        readOutput();
        QStringList args = parseConf(CONF_RUN,"",isOptimisation);
        Solver* s = getCurrentSolver();
        if (!s->executable.isEmpty() && s->executable_resolved.isEmpty()) {
            QMessageBox::warning(this,"MiniZinc IDE","The solver "+s->executable+" cannot be executed.",
                                 QMessageBox::Ok);
            delete tmpDir;
            tmpDir = NULL;
            procFinished(exitcode);
            return;
        }
        args << currentFznTarget;

        if (s->isGUIApplication) {
            addOutput("<div class='mznnotice'>Running "+curEditor->filename+" (detached)</div>");

            MznProcess* detached_process = new MznProcess(this);
            detached_process->setWorkingDirectory(QFileInfo(curEditor->filepath).absolutePath());

            QString executable = s->executable_resolved;
            if (ui->conf_solver_verbose->isChecked()) {
                addOutput("<div class='mznnotice'>Command line:</div>");
                QString cmdline = executable;
                QRegExp white("\\s");
                for (int i=0; i<args.size(); i++) {
                    if (white.indexIn(args[i]) != -1)
                        cmdline += " \""+args[i]+"\"";
                    else
                        cmdline += " "+args[i];
                }
                addOutput("<div>"+cmdline+"</div>");
            }
            detached_process->start(executable,args,getMznDistribPath());
            cleanupTmpDirs.append(tmpDir);
            cleanupProcesses.append(detached_process);
            tmpDir = NULL;
            procFinished(exitcode);
        } else {
            if (runSolns2Out) {
                outputProcess = new MznProcess(this);
                outputProcess->setWorkingDirectory(QFileInfo(curFilePath).absolutePath());
                connect(outputProcess, SIGNAL(readyReadStandardOutput()), this, SLOT(readOutput()));
                connect(outputProcess, SIGNAL(readyReadStandardError()), this, SLOT(readOutput()));
                connect(outputProcess, SIGNAL(error(QProcess::ProcessError)),
                        this, SLOT(outputProcError(QProcess::ProcessError)));
            }
            process = new MznProcess(this);
            processName = s->executable_resolved;
            processWasStopped = false;
            process->setWorkingDirectory(QFileInfo(curFilePath).absolutePath());
            if (runSolns2Out) {
                process->setStandardOutputProcess(outputProcess);
            } else {
                connect(process, SIGNAL(readyReadStandardOutput()), this, SLOT(readOutput()));
            }
            connect(process, SIGNAL(readyReadStandardError()), this, SLOT(readOutput()));
            connect(process, SIGNAL(finished(int)), this, SLOT(procFinished(int)));
            connect(process, SIGNAL(error(QProcess::ProcessError)),
                    this, SLOT(procError(QProcess::ProcessError)));
            int remainingTime = solverTimeout->remainingTime();
            if (remainingTime > 0) {
                if (s->stdFlags.contains("-t")) {
                    args.push_front(QString().number(remainingTime));
                    args.push_front("-t");
                    // add one second to give solver a chance to quit
                    solverTimeout->start(remainingTime+1000);
                }
            }

            addOutput("<div class='mznnotice'>Running "+QFileInfo(curFilePath).fileName()+"</div>");
            QString executable = s->executable_resolved;
            if (ui->conf_solver_verbose->isChecked()) {
                addOutput("<div class='mznnotice'>Command line:</div>");
                QString cmdline = executable;
                QRegExp white("\\s");
                for (int i=0; i<args.size(); i++) {
                    if (white.indexIn(args[i]) != -1)
                        cmdline += " \""+args[i]+"\"";
                    else
                        cmdline += " "+args[i];
                }
                addOutput("<div>"+cmdline+"</div>");
            }
            process->start(executable,args,getMznDistribPath());
            if (runSolns2Out) {
                QStringList outargs;
                if (ui->conf_solver_timing->isChecked()) {
                    outargs << "--output-time";
                }
                outargs << "--ozn-file" << currentFznTarget.left(currentFznTarget.length()-4)+".ozn";
                outputProcess->start(minizinc_executable,outargs,getMznDistribPath());
            }
            time = 0;
            timer->start(500);
        }
    } else {
        delete tmpDir;
        tmpDir = NULL;
        procFinished(exitcode);
    }
}

void MainWindow::on_actionCompile_triggered()
{
    compileOnly = true;
    compileOrRun();
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
                       "Copyright Monash University, NICTA, Data61 2013-2018\n\n"+
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
          throw -1;
        }
      }
    } else {
       CodeEditor* ce = static_cast<CodeEditor*>(ui->tabWidget->widget(ui->tabWidget->currentIndex()));
       ces[p] = ce;
    }
  }
  return ces;
}

void MainWindow::updateSolverConfigs()
{
    QString curText = ui->conf_solver_conf->currentText();
    ui->conf_solver_conf->clear();
    ui->menuSolver_configurations->clear();
    solverConfCombo->clear();
    int idx = -1;
    currentSolverConfig = -1;
    for (int i=0; i<projectSolverConfigs.size(); i++) {
        ui->conf_solver_conf->addItem(projectSolverConfigs[i].name);
        solverConfCombo->addItem(projectSolverConfigs[i].name);
        QAction* solverConfAction = ui->menuSolver_configurations->addAction(projectSolverConfigs[i].name);
        solverConfAction->setCheckable(true);
        if (projectSolverConfigs[i].name==curText) {
            idx = i;
            solverConfAction->setChecked(true);
        }
    }
    if (!projectSolverConfigs.empty()) {
        ui->conf_solver_conf->insertSeparator(projectSolverConfigs.size());
        solverConfCombo->insertSeparator(projectSolverConfigs.size());
        ui->menuSolver_configurations->addSeparator();
    }
    for (int i=0; i<builtinSolverConfigs.size(); i++) {
        QString scn = builtinSolverConfigs[i].name+(" [built-in]");
        ui->conf_solver_conf->addItem(scn);
        solverConfCombo->addItem(scn);
        QAction* solverConfAction = ui->menuSolver_configurations->addAction(scn);
        solverConfAction->setCheckable(true);
        if (scn==curText) {
            idx = i+projectSolverConfigs.size();
            solverConfAction->setChecked(true);
        }
    }
    ui->menuSolver_configurations->addSeparator();
    ui->menuSolver_configurations->addAction(ui->actionEditSolverConfig);
    setCurrentSolverConfig(idx);
}

void clearLayout(QLayout* l) {
    QLayoutItem *toRemove;
    while ((toRemove = l->takeAt(0)) != 0) {
        if (toRemove->layout())
            clearLayout(toRemove->layout());
        delete toRemove->widget();
        delete toRemove;
    }

}

void MainWindow::setCurrentSolverConfig(int idx)
{
    if (idx==-1 || idx >= projectSolverConfigs.size()+builtinSolverConfigs.size())
        return;
    int actionIdx = (projectSolverConfigs.size()!=0 && idx >= projectSolverConfigs.size()) ? idx+1 : idx;
    ui->conf_solver_conf->setCurrentIndex(actionIdx);
    solverConfCombo->setCurrentIndex(actionIdx);
    QList<QAction*> actions = ui->menuSolver_configurations->actions();
    for (int i=0; i<actions.size(); i++) {
        actions[i]->setChecked(i==actionIdx);
    }

    if (currentSolverConfig != -1) {
        if (currentSolverConfig < projectSolverConfigs.size() || currentSolverConfig-projectSolverConfigs.size() < builtinSolverConfigs.size()) {
            SolverConfiguration& oldConf = currentSolverConfig < projectSolverConfigs.size() ? projectSolverConfigs[currentSolverConfig] : builtinSolverConfigs[currentSolverConfig-projectSolverConfigs.size()];
            oldConf.timeLimit = ui->conf_timeLimit->value();
            oldConf.defaultBehaviour = ui->defaultBehaviourButton->isChecked();
            oldConf.printIntermediate = ui->conf_printall->isChecked();
            oldConf.stopAfter = ui->conf_nsol->value();
            oldConf.compressSolutionOutput = ui->conf_compressSolutionLimit->value();
            oldConf.clearOutputWindow = ui->autoclear_output->isChecked();
            oldConf.verboseFlattening = ui->conf_verbose->isChecked();
            oldConf.flatteningStats = ui->conf_flatten_stats->isChecked();
            oldConf.optimizationLevel = ui->conf_optlevel->currentIndex();
            oldConf.additionalData = ui->conf_cmd_params->text();
            oldConf.additionalCompilerCommandline = ui->conf_mzn2fzn_params->text();
            oldConf.nThreads = ui->conf_nthreads->value();
            oldConf.randomSeed = ui->conf_seed->text().isEmpty() ? QVariant() : ui->conf_seed->text().toInt();
            oldConf.solverFlags = ui->conf_solverFlags->text();
            oldConf.freeSearch = ui->conf_solver_free->isChecked();
            oldConf.verboseSolving = ui->conf_solver_verbose->isChecked();
            oldConf.solvingStats = ui->conf_stats->isChecked();
            oldConf.runSolutionChecker = ui->conf_check_solutions->isChecked();
            oldConf.extraOptions.clear();
            for (auto& ef : extraSolverFlags) {
                switch (ef.first.t) {
                case SolverFlag::T_BOOL:
                    oldConf.extraOptions[ef.first.name] = static_cast<QCheckBox*>(ef.second)->isChecked() ? "true" : "false";
                    break;
                case SolverFlag::T_OPT:
                case SolverFlag::T_SOLVER:
                    oldConf.extraOptions[ef.first.name] = static_cast<QComboBox*>(ef.second)->currentText();
                    break;
                case SolverFlag::T_INT:
                case SolverFlag::T_FLOAT:
                case SolverFlag::T_STRING:
                    oldConf.extraOptions[ef.first.name] = static_cast<QLineEdit*>(ef.second)->text();
                    break;
                }
            }
        }
    }
    currentSolverConfig = idx;
    SolverConfiguration& conf = idx < projectSolverConfigs.size() ? projectSolverConfigs[idx] : builtinSolverConfigs[idx-projectSolverConfigs.size()];

    int solverIdx = 0;
    for (int i=solvers.size(); i--;) {
        Solver& s = solvers[i];
        if (s.id==conf.solverId) {
            solverIdx = i;
            if (s.version==conf.solverVersion)
                break;
        }
    }
    if (solverIdx < solvers.size()) {
        ui->conf_solver->setText(solvers[solverIdx].name+" "+solvers[solverIdx].version);
    }
    ui->conf_timeLimit->setValue(conf.timeLimit);
    ui->defaultBehaviourButton->setChecked(conf.defaultBehaviour);
    ui->userBehaviourButton->setChecked(!conf.defaultBehaviour);
    ui->conf_printall->setChecked(conf.printIntermediate);
    ui->conf_nsol->setValue(conf.stopAfter);
    ui->conf_compressSolutionLimit->setValue(conf.compressSolutionOutput);
    ui->autoclear_output->setChecked(conf.clearOutputWindow);
    ui->conf_verbose->setChecked(conf.verboseFlattening);
    ui->conf_flatten_stats->setChecked(conf.flatteningStats);
    ui->conf_optlevel->setCurrentIndex(conf.optimizationLevel);
    ui->conf_cmd_params->setText(conf.additionalData);
    ui->conf_mzn2fzn_params->setText(conf.additionalCompilerCommandline);
    ui->conf_nthreads->setValue(conf.nThreads);
    ui->conf_seed->setText(conf.randomSeed.isValid() ? QString().number(conf.randomSeed.toInt()) : QString());
    ui->conf_solverFlags->setText(conf.solverFlags);
    ui->conf_solver_free->setChecked(conf.freeSearch);
    ui->conf_solver_verbose->setChecked(conf.verboseSolving);
    ui->conf_stats->setChecked(conf.solvingStats);
    ui->conf_check_solutions->setChecked(conf.runSolutionChecker);

    // Remove all extra options
    clearLayout(ui->extraOptionsLayout);
    if (solverIdx < solvers.size()) {
        Solver& currentSolver = solvers[solverIdx];
        ui->conf_nthreads->setEnabled(currentSolver.stdFlags.contains("-p"));
        ui->nthreads_label->setEnabled(currentSolver.stdFlags.contains("-p"));
        ui->conf_seed->setEnabled(currentSolver.stdFlags.contains("-r"));
        ui->conf_have_seed->setEnabled(currentSolver.stdFlags.contains("-r"));
        ui->conf_stats->setEnabled(currentSolver.stdFlags.contains("-s"));
        ui->conf_printall->setEnabled(currentSolver.stdFlags.contains("-a"));
        ui->conf_nsol->setEnabled(currentSolver.stdFlags.contains("-n"));
        ui->nsol_label_1->setEnabled(currentSolver.stdFlags.contains("-n"));
        ui->nsol_label_2->setEnabled(currentSolver.stdFlags.contains("-n"));
        ui->conf_solver_free->setEnabled(currentSolver.stdFlags.contains("-f"));
        ui->conf_solver_verbose->setEnabled((!currentSolver.supportsMzn && !currentSolver.executable.isEmpty()) ||
                                            currentSolver.stdFlags.contains("-v"));

        ui->extraOptionsBox->setVisible(!currentSolver.extraFlags.empty());
        extraSolverFlags.clear();
        for (auto f : currentSolver.extraFlags) {
            switch (f.t) {
            case SolverFlag::T_INT:
            {
                QHBoxLayout* hb = new QHBoxLayout();
                QLineEdit* le = new QLineEdit(this);
                le->setText(conf.extraOptions.contains(f.name) ? conf.extraOptions[f.name] : f.def);
                le->setValidator(new QIntValidator(this));
                hb->addWidget(new QLabel(f.description));
                hb->addWidget(le);
                ui->extraOptionsLayout->addLayout(hb);
                extraSolverFlags.push_back(qMakePair(f,le));
            }
                break;
            case SolverFlag::T_BOOL:
            {
                QCheckBox* cb = new QCheckBox(f.description,this);
                cb->setChecked(conf.extraOptions.contains(f.name) ? (conf.extraOptions[f.name]=="true") : (f.def=="true"));
                ui->extraOptionsLayout->addWidget(cb);
                extraSolverFlags.push_back(qMakePair(f,cb));
            }
                break;
            case SolverFlag::T_FLOAT:
            {
                QHBoxLayout* hb = new QHBoxLayout();
                QLineEdit* le = new QLineEdit(this);
                le->setText(conf.extraOptions.contains(f.name) ? conf.extraOptions[f.name] : f.def);
                le->setValidator(new QDoubleValidator(this));
                hb->addWidget(new QLabel(f.description));
                hb->addWidget(le);
                ui->extraOptionsLayout->addLayout(hb);
                extraSolverFlags.push_back(qMakePair(f,le));
            }
                break;
            case SolverFlag::T_STRING:
            {
                QHBoxLayout* hb = new QHBoxLayout();
                QLineEdit* le = new QLineEdit(this);
                le->setText(conf.extraOptions.contains(f.name) ? conf.extraOptions[f.name] : f.def);
                hb->addWidget(new QLabel(f.description));
                hb->addWidget(le);
                ui->extraOptionsLayout->addLayout(hb);
                extraSolverFlags.push_back(qMakePair(f,le));
            }
                break;
            case SolverFlag::T_OPT:
            {
                QHBoxLayout* hb = new QHBoxLayout();
                QComboBox* cb = new QComboBox(this);
                for (auto o : f.options) {
                    cb->addItem(o);
                }
                cb->setCurrentText(conf.extraOptions.contains(f.name) ? conf.extraOptions[f.name] : f.def);
                hb->addWidget(new QLabel(f.description));
                hb->addWidget(cb);
                hb->addItem(new QSpacerItem(0,0,QSizePolicy::Expanding));
                ui->extraOptionsLayout->addLayout(hb);
                extraSolverFlags.push_back(qMakePair(f,cb));
            }
                break;
            case SolverFlag::T_SOLVER:
            {
                QHBoxLayout* hb = new QHBoxLayout();
                QComboBox* cb = new QComboBox(this);
                for (auto s : projectSolverConfigs) {
                    cb->addItem(s.name);
                }
                for (auto s : builtinSolverConfigs) {
                    cb->addItem(s.name);
                }
                cb->setCurrentText(conf.extraOptions.contains(f.name) ? conf.extraOptions[f.name] : f.def);
                hb->addWidget(new QLabel(f.description));
                hb->addWidget(cb);
                hb->addItem(new QSpacerItem(0,0,QSizePolicy::Expanding));
                ui->extraOptionsLayout->addLayout(hb);
                extraSolverFlags.push_back(qMakePair(f,cb));
            }
                break;
            }
        }
    }

    if (idx < projectSolverConfigs.size()) {
        ui->conf_default->hide();
    } else {
        ui->conf_default->setChecked(idx-projectSolverConfigs.size()==defaultSolverIdx);
        ui->conf_default->setEnabled(idx-projectSolverConfigs.size()!=defaultSolverIdx);
        ui->conf_default->show();
    }

    bool haveChecker = false;
    if (curEditor!=NULL && curEditor->filename.endsWith(".mzn")) {
        QString checkFile = curEditor->filepath;
        checkFile.replace(checkFile.length()-1,1,"c");
        haveChecker = project.containsFile(checkFile) || project.containsFile(checkFile+".mzn");
    }
    if (haveChecker && (ui->defaultBehaviourButton->isChecked() || ui->conf_check_solutions->isChecked())) {
        ui->actionRun->setText("Run + check");
    } else {
        ui->actionRun->setText("Run");
    }

    runButton->setToolTip("Run "+conf.name);

    if (conf.isBuiltin)
        ui->solverConfType->setText("built-in configuration");
    else
        ui->solverConfType->setText("configuration in current project");

    ui->groupBox_2->setEnabled(true);
    ui->groupBox_3->setEnabled(true);
    ui->cloneSolverConfButton->setEnabled(true);
    ui->deleteSolverConfButton->setEnabled(true);
    ui->deleteSolverConfButton->setText(conf.isBuiltin ? "Reset to defaults" : "Delete");
    if (conf.isBuiltin) {
        ui->renameSolverConfButton->hide();
    } else {
        ui->renameSolverConfButton->show();
        ui->renameSolverConfButton->setEnabled(true);
    }
    project.solverConfigs(projectSolverConfigs,false);
}

void MainWindow::find(bool fwd)
{
    const QString& toFind = ui->find->text();
    QTextDocument::FindFlags flags;
    if (!fwd)
        flags |= QTextDocument::FindBackward;
    bool ignoreCase = ui->check_case->isChecked();
    if (!ignoreCase)
        flags |= QTextDocument::FindCaseSensitively;
    bool wrap = ui->check_wrap->isChecked();

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

void MainWindow::on_conf_solver_conf_currentIndexChanged(int index)
{
    if (projectSolverConfigs.size() != 0 && index >= projectSolverConfigs.size())
        index--; // don't count separator
    setCurrentSolverConfig(index);
}

void MainWindow::on_solverConfigurationSelected(QAction* action)
{
    if (action==ui->actionEditSolverConfig)
        return;
    QList<QAction*> actions = ui->menuSolver_configurations->actions();
    for (int i=0; i<actions.size(); i++) {
        if (action==actions[i]) {
            return on_conf_solver_conf_currentIndexChanged(i);
        }
    }
}

void MainWindow::on_cloneSolverConfButton_clicked()
{
    QString cur = ui->conf_solver_conf->currentText();
    cur = cur.replace(" [built-in]","");
    int clone = 1;
    QRegExp re("Clone (\\d+) of (.*)");
    int pos = re.indexIn(cur);
    if (pos != -1) {
        clone = re.cap(1).toInt();
        cur = re.cap(2);
    }
    while (ui->conf_solver_conf->findText("Clone "+QString().number(clone)+" of "+cur) != -1)
        clone++;
    ui->solverConfNameEdit->setText("Clone "+QString().number(clone)+" of "+cur);
    ui->solverConfNameEdit->show();
    ui->solverConfNameEdit->setFocus();
    ui->nameAlreadyUsedLabel->hide();
    ui->solverConfType->hide();
    ui->conf_solver_conf->hide();
    ui->groupBox_2->setEnabled(false);
    ui->groupBox_3->setEnabled(false);
    ui->deleteSolverConfButton->setEnabled(false);
    ui->cloneSolverConfButton->setEnabled(false);
    ui->renameSolverConfButton->setEnabled(false);
    renamingSolverConf = false;
}

void MainWindow::on_deleteSolverConfButton_clicked()
{
    SolverConfiguration& oldConf = currentSolverConfig < projectSolverConfigs.size() ? projectSolverConfigs[currentSolverConfig] : builtinSolverConfigs[currentSolverConfig-projectSolverConfigs.size()];
    if (oldConf.isBuiltin) {
        // reset builtin configuration to default values
        SolverConfiguration newConf = SolverConfiguration::defaultConfig();
        newConf.name = oldConf.name;
        newConf.solverId = oldConf.solverId;
        newConf.solverVersion = oldConf.solverVersion;
        oldConf = newConf;
        currentSolverConfig = -1;
        updateSolverConfigs();
    } else {
        QString curSolver = ui->conf_solver_conf->currentText();

        QMessageBox msg;
        msg.setText("Do you really want to delete the solver configuration \""+curSolver+"\"?");
        msg.setStandardButtons(QMessageBox::Yes| QMessageBox::Cancel);
        msg.setDefaultButton(QMessageBox::Cancel);
        if (msg.exec()==QMessageBox::Yes) {
            int idx = ui->conf_solver_conf->currentIndex();
            if (idx<projectSolverConfigs.size()) {
                projectSolverConfigs.remove(idx);
            }
            updateSolverConfigs();
            setCurrentSolverConfig(0);
        }
    }
}

void MainWindow::on_renameSolverConfButton_clicked()
{
    QString cur = ui->conf_solver_conf->currentText();
    ui->solverConfNameEdit->setText(cur);
    ui->solverConfNameEdit->show();
    ui->solverConfNameEdit->setFocus();
    ui->nameAlreadyUsedLabel->hide();
    ui->solverConfType->hide();
    ui->conf_solver_conf->hide();
    ui->groupBox_2->setEnabled(false);
    ui->groupBox_3->setEnabled(false);
    ui->deleteSolverConfButton->setEnabled(false);
    ui->cloneSolverConfButton->setEnabled(false);
    ui->renameSolverConfButton->setEnabled(false);
    renamingSolverConf = true;
}

void MainWindow::on_solverConfNameEdit_returnPressed()
{
    QString newName = ui->solverConfNameEdit->text();
    if (renamingSolverConf) {
        QString prevName = ui->conf_solver_conf->currentText();
        if (newName==prevName)
            on_solverConfNameEdit_escPressed();
        if (!newName.isEmpty() && ui->conf_solver_conf->findText(newName)==-1) {
            ui->solverConfNameEdit->hide();
            ui->nameAlreadyUsedLabel->hide();
            ui->solverConfType->show();
            ui->conf_solver_conf->show();
            int idx = ui->conf_solver_conf->currentIndex();
            if (projectSolverConfigs.size()!=0 && idx > projectSolverConfigs.size())
                idx--;
            SolverConfiguration& conf = idx < projectSolverConfigs.size() ? projectSolverConfigs[idx] : builtinSolverConfigs[idx-projectSolverConfigs.size()];
            conf.name = newName;
            updateSolverConfigs();
            on_conf_solver_conf_currentIndexChanged(idx);
        } else {
            ui->nameAlreadyUsedLabel->show();
            ui->solverConfNameEdit->setFocus();
        }
    } else {
        if (!newName.isEmpty() && ui->conf_solver_conf->findText(newName)==-1) {
            ui->solverConfNameEdit->hide();
            ui->nameAlreadyUsedLabel->hide();
            ui->solverConfType->show();
            ui->conf_solver_conf->show();
            int idx = ui->conf_solver_conf->currentIndex();
            if (projectSolverConfigs.size()!=0 && idx > projectSolverConfigs.size())
                idx--;
            SolverConfiguration newConf = idx < projectSolverConfigs.size() ? projectSolverConfigs[idx] : builtinSolverConfigs[idx-projectSolverConfigs.size()];
            newConf.name = newName;
            newConf.isBuiltin = false;
            projectSolverConfigs.push_front(newConf);
            updateSolverConfigs();
            setCurrentSolverConfig(0);
        } else {
            ui->nameAlreadyUsedLabel->show();
            ui->solverConfNameEdit->setFocus();
        }
    }
}

void MainWindow::on_solverConfNameEdit_escPressed()
{
    ui->solverConfNameEdit->hide();
    ui->nameAlreadyUsedLabel->hide();
    ui->solverConfType->hide();
    ui->conf_solver_conf->show();
    on_conf_solver_conf_currentIndexChanged(ui->conf_solver_conf->currentIndex());
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

    SolverDialog sd(solvers,userSolverConfigDir,userConfigFile,mznStdlibDir,addNew,mznDistribPath);
    sd.exec();
    mznDistribPath = sd.mznPath();
    if (!mznDistribPath.isEmpty() && ! (mznDistribPath.endsWith("/") || mznDistribPath.endsWith("\\")))
        mznDistribPath += "/";
    checkMznPath();
    SolverConfiguration::defaultConfigs(solvers, builtinSolverConfigs, defaultSolverIdx);
    updateSolverConfigs();

    settings.beginGroup("ide");
    if (!checkUpdates && settings.value("checkforupdates21",false).toBool()) {
        settings.setValue("lastCheck21",QDate::currentDate().addDays(-2).toString());
        IDE::instance()->checkUpdate();
    }
    settings.endGroup();

    settings.beginGroup("minizinc");
    settings.setValue("mznpath",mznDistribPath);
    settings.endGroup();

}


void MainWindow::on_conf_default_toggled(bool checked)
{
    if (checked) {
        ui->conf_default->setEnabled(false);
        int newDefaultSolverIdx = projectSolverConfigs.size()==0 ? ui->conf_solver_conf->currentIndex() : ui->conf_solver_conf->currentIndex()-projectSolverConfigs.size()-1;
        if (newDefaultSolverIdx==defaultSolverIdx)
            return;
        QString newDefaultSolver = builtinSolverConfigs[newDefaultSolverIdx].solverId;
        QFile uc(userConfigFile);
        QJsonObject jo;
        if (uc.exists()) {
            if (uc.open(QFile::ReadOnly)) {
                QJsonDocument doc = QJsonDocument::fromJson(uc.readAll());
                if (doc.isNull()) {
                    QMessageBox::warning(this,"MiniZinc IDE","Cannot modify user configuration file "+userConfigFile,QMessageBox::Ok);
                    return;
                }
                jo = doc.object();
                uc.close();
            }
        }
        QJsonArray tagdefs = jo.contains("tagDefaults") ? jo["tagDefaults"].toArray() : QJsonArray();
        bool hadDefault = false;
        for (int i=0; i<tagdefs.size(); i++) {
            if (tagdefs[i].isArray() && tagdefs[i].toArray()[0].isString() && tagdefs[i].toArray()[0].toString().isEmpty()) {
                QJsonArray def = tagdefs[i].toArray();
                def[1] = newDefaultSolver;
                tagdefs[i] = def;
                hadDefault = true;
                break;
            }
        }
        if (!hadDefault) {
            QJsonArray def;
            def.append("");
            def.append(newDefaultSolver);
            tagdefs.append(def);
        }
        jo["tagDefaults"] = tagdefs;
        QJsonDocument doc;
        doc.setObject(jo);
        QFileInfo uc_info(userConfigFile);
        if (!QDir().mkpath(uc_info.absoluteDir().absolutePath())) {
            QMessageBox::warning(this,"MiniZinc IDE","Cannot create user configuration directory "+uc_info.absoluteDir().absolutePath(),QMessageBox::Ok);
            return;
        }
        if (uc.open(QFile::ReadWrite | QIODevice::Truncate)) {
            uc.write(doc.toJson());
            uc.close();
        } else {
            QMessageBox::warning(this,"MiniZinc IDE","Cannot write user configuration file "+userConfigFile,QMessageBox::Ok);
            return;
        }
        defaultSolverIdx = newDefaultSolverIdx;
    }
}

void MainWindow::on_actionFind_triggered()
{
    incrementalFindCursor = curEditor->textCursor();
    incrementalFindCursor.setPosition(std::min(curEditor->textCursor().anchor(), curEditor->textCursor().position()));
    if (curEditor->textCursor().hasSelection()) {
        ui->find->setText(curEditor->textCursor().selectedText());
    }
    ui->not_found->setText("");
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
    if (curEditor==NULL)
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

void MainWindow::checkMznPath()
{
    QString ignoreVersionString;
    SolverDialog::checkMznExecutable(mznDistribPath,minizinc_executable,ignoreVersionString,solvers,userSolverConfigDir,userConfigFile,mznStdlibDir);

    if (minizinc_executable.isEmpty()) {
        int ret = QMessageBox::warning(this,"MiniZinc IDE","Could not find the minizinc executable.\nDo you want to open the settings dialog?",
                                       QMessageBox::Ok | QMessageBox::Cancel);
        if (ret == QMessageBox::Ok)
            on_actionManage_solvers_triggered();
    }
    bool haveMzn = (!minizinc_executable.isEmpty() && solvers.size() > 0);
    ui->actionRun->setEnabled(haveMzn);
    ui->actionCompile->setEnabled(haveMzn);
    ui->actionEditSolverConfig->setEnabled(haveMzn);
    ui->actionSubmit_to_MOOC->setEnabled(haveMzn);
    if (!haveMzn)
        ui->conf_dock_widget->hide();
}

void MainWindow::on_actionShift_left_triggered()
{
    QTextCursor cursor = curEditor->textCursor();
    QTextBlock block = curEditor->document()->findBlock(cursor.selectionStart());
    QTextBlock endblock = curEditor->document()->findBlock(cursor.selectionEnd());
    if (block==endblock || !cursor.atBlockStart())
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
    if (block==endblock || !cursor.atBlockStart())
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
    QString filepath = f;
    if (filepath.isEmpty()) {
        filepath = QFileDialog::getSaveFileName(this,"Save project",getLastPath(),"MiniZinc projects (*.mzp)");
        if (!filepath.isNull()) {
            setLastPath(QFileInfo(filepath).absolutePath()+fileDialogSuffix);
        }
    }
    if (!filepath.isEmpty()) {
        if (projectPath != filepath && IDE::instance()->projects.contains(filepath)) {
            QMessageBox::warning(this,"MiniZinc IDE","Cannot overwrite existing open project.",
                                 QMessageBox::Ok);
            return;
        }
        QFile file(filepath);
        if (file.open(QFile::WriteOnly | QFile::Text)) {
            if (projectPath != filepath) {
                IDE::instance()->projects.remove(projectPath);
                IDE::instance()->projects.insert(filepath,this);
                project.setRoot(ui->projectView, projectSort, filepath);
                projectPath = filepath;
            }
            updateRecentProjects(projectPath);
            tabChange(ui->tabWidget->currentIndex());

            QJsonObject confObject;

            confObject["version"] = 105;

            QStringList openFiles;
            QDir projectDir = QFileInfo(filepath).absoluteDir();
            for (int i=0; i<ui->tabWidget->count(); i++) {
                CodeEditor* ce = static_cast<CodeEditor*>(ui->tabWidget->widget(i));
                if (!ce->filepath.isEmpty())
                    openFiles << projectDir.relativeFilePath(ce->filepath);
            }
            confObject["openFiles"] = QJsonArray::fromStringList(openFiles);
            confObject["openTab"] = ui->tabWidget->currentIndex();

            QStringList projectFilesRelPath;
            QStringList projectFiles = project.files();
            for (QList<QString>::const_iterator it = projectFiles.begin();
                 it != projectFiles.end(); ++it) {
                projectFilesRelPath << projectDir.relativeFilePath(*it);
            }
            confObject["projectFiles"] = QJsonArray::fromStringList(projectFilesRelPath);

            // Make sure any configuration changes have been updated
            on_conf_solver_conf_currentIndexChanged(ui->conf_solver_conf->currentIndex());

            // Save all project solver configurations
            QJsonArray projectConfigs;
            for (int i=0; i<projectSolverConfigs.size(); i++) {
                SolverConfiguration& sc = projectSolverConfigs[i];
                QJsonObject projConf;
                projConf["name"] = sc.name;
                projConf["id"] = sc.solverId;
                projConf["version"] = sc.solverVersion;
                projConf["timeLimit"] = sc.timeLimit;
                projConf["defaultBehavior"] = sc.defaultBehaviour;
                projConf["printIntermediate"] = sc.printIntermediate;
                projConf["stopAfter"] = sc.stopAfter;
                projConf["compressSolutionOutput"] = sc.compressSolutionOutput;
                projConf["clearOutputWindow"] = sc.clearOutputWindow;
                projConf["verboseFlattening"] = sc.verboseFlattening;
                projConf["flatteningStats"] = sc.flatteningStats;
                projConf["optimizationLevel"] = sc.optimizationLevel;
                projConf["additionalData"] = sc.additionalData;
                projConf["additionalCompilerCommandline"] = sc.additionalCompilerCommandline;
                projConf["nThreads"] = sc.nThreads;
                if (sc.randomSeed.isValid()) {
                    projConf["randomSeed"] = sc.randomSeed.toInt();
                }
                projConf["solverFlags"] = sc.solverFlags;
                projConf["freeSearch"] = sc.freeSearch;
                projConf["verboseSolving"] = sc.verboseSolving;
                projConf["outputTiming"] = sc.outputTiming;
                projConf["solvingStats"] = sc.solvingStats;
                projConf["runSolutionChecker"] = sc.runSolutionChecker;
                if (!sc.extraOptions.empty()) {
                    QJsonObject extraOptions;
                    for (auto& k : sc.extraOptions.keys()) {
                        extraOptions[k] = sc.extraOptions[k];
                    }
                    projConf["extraOptions"] = extraOptions;
                }
                projectConfigs.append(projConf);
            }
            confObject["projectSolverConfigs"] = projectConfigs;
            // Save all modified built-in solver configurations
            QJsonArray builtinConfigs;
            for (int i=0; i<builtinSolverConfigs.size(); i++) {
                SolverConfiguration& sc = builtinSolverConfigs[i];

                SolverConfiguration defConf = SolverConfiguration::defaultConfig();
                defConf.name = sc.name;
                defConf.solverId = sc.solverId;
                defConf.solverVersion = sc.solverVersion;
                if (!(defConf==sc)) {
                    QJsonObject projConf;
                    projConf["name"] = sc.name;
                    projConf["id"] = sc.solverId;
                    projConf["version"] = sc.solverVersion;
                    projConf["timeLimit"] = sc.timeLimit;
                    projConf["defaultBehavior"] = sc.defaultBehaviour;
                    projConf["printIntermediate"] = sc.printIntermediate;
                    projConf["stopAfter"] = sc.stopAfter;
                    projConf["compressSolutionOutput"] = sc.compressSolutionOutput;
                    projConf["clearOutputWindow"] = sc.clearOutputWindow;
                    projConf["verboseFlattening"] = sc.verboseFlattening;
                    projConf["flatteningStats"] = sc.flatteningStats;
                    projConf["optimizationLevel"] = sc.optimizationLevel;
                    projConf["additionalData"] = sc.additionalData;
                    projConf["additionalCompilerCommandline"] = sc.additionalCompilerCommandline;
                    projConf["nThreads"] = sc.nThreads;
                    if (sc.randomSeed.isValid()) {
                        projConf["randomSeed"] = sc.randomSeed.toInt();
                    }
                    projConf["solverFlags"] = sc.solverFlags;
                    projConf["freeSearch"] = sc.freeSearch;
                    projConf["verboseSolving"] = sc.verboseSolving;
                    projConf["outputTiming"] = sc.outputTiming;
                    projConf["solvingStats"] = sc.solvingStats;
                    projConf["runSolutionChecker"] = sc.runSolutionChecker;
                    if (!sc.extraOptions.empty()) {
                        QJsonObject extraOptions;
                        for (auto& k : sc.extraOptions.keys()) {
                            extraOptions[k] = sc.extraOptions[k];
                        }
                        projConf["extraOptions"] = extraOptions;
                    }
                    builtinConfigs.append(projConf);
                }
            }
            confObject["builtinSolverConfigs"] = builtinConfigs;

            int scIdx = ui->conf_solver_conf->currentIndex();
            if (scIdx < projectSolverConfigs.size()) {
                // currently selected config has been saved to the project
                confObject["selectedProjectConfig"] = scIdx;
            } else {
                if (projectSolverConfigs.size()!=0)
                    scIdx--;
                SolverConfiguration& curSc = builtinSolverConfigs[scIdx-projectSolverConfigs.size()];
                confObject["selectedBuiltinConfigId"] = curSc.solverId;
                confObject["selectedBuiltinConfigVersion"] = curSc.solverVersion;
            }

            QJsonDocument jdoc(confObject);
            file.write(jdoc.toJson());
            file.close();
        } else {
            QMessageBox::warning(this,"MiniZinc IDE","Could not save project");
        }
    }
}

namespace {
    SolverConfiguration scFromJson(QJsonObject sco) {
        SolverConfiguration newSc;
        if (sco["name"].isString()) {
            newSc.name = sco["name"].toString();
        }
        if (sco["id"].isString()) {
            newSc.solverId = sco["id"].toString();
        }
        if (sco["version"].isString()) {
            newSc.solverVersion = sco["version"].toString();
        }
        if (sco["timeLimit"].isDouble()) {
            newSc.timeLimit = sco["timeLimit"].toDouble();
        }
        if (sco["defaultBehavior"].isBool()) {
            newSc.defaultBehaviour = sco["defaultBehavior"].toBool();
        }
        if (sco["printIntermediate"].isBool()) {
            newSc.printIntermediate = sco["printIntermediate"].toBool();
        }
        if (sco["stopAfter"].isDouble()) {
            newSc.stopAfter = sco["stopAfter"].toDouble();
        }
        if (sco["compressSolutionOutput"].isDouble()) {
            newSc.compressSolutionOutput = sco["compressSolutionOutput"].toDouble();
        }
        if (sco["clearOutputWindow"].isBool()) {
            newSc.clearOutputWindow = sco["clearOutputWindow"].toBool();
        }
        if (sco["verboseFlattening"].isBool()) {
            newSc.verboseFlattening = sco["verboseFlattening"].toBool();
        }
        if (sco["flatteningStats"].isBool()) {
            newSc.flatteningStats = sco["flatteningStats"].toBool();
        }
        if (sco["optimizationLevel"].isDouble()) {
            newSc.optimizationLevel = sco["optimizationLevel"].toDouble();
        }
        if (sco["additionalData"].isString()) {
            newSc.additionalData = sco["additionalData"].toString();
        }
        if (sco["additionalCompilerCommandline"].isString()) {
            newSc.additionalCompilerCommandline = sco["additionalCompilerCommandline"].toString();
        }
        if (sco["nThreads"].isDouble()) {
            newSc.nThreads = sco["nThreads"].toDouble();
        }
        if (sco["randomSeed"].isDouble()) {
            newSc.randomSeed = sco["randomSeed"].toDouble();
        }
        if (sco["solverFlags"].isString()) {
            newSc.solverFlags = sco["solverFlags"].toString();
        }
        if (sco["freeSearch"].isBool()) {
            newSc.freeSearch = sco["freeSearch"].toBool();
        }
        if (sco["verboseSolving"].isBool()) {
            newSc.verboseSolving = sco["verboseSolving"].toBool();
        }
        if (sco["outputTiming"].isBool()) {
            newSc.outputTiming = sco["outputTiming"].toBool();
        }
        if (sco["solvingStats"].isBool()) {
            newSc.solvingStats = sco["solvingStats"].toBool();
        }
        if (sco["runSolutionChecker"].toBool()) {
            newSc.runSolutionChecker = sco["runSolutionChecker"].toBool();
        }
        if (sco["extraOptions"].isObject()) {
            QJsonObject extraOptions = sco["extraOptions"].toObject();
            for (auto& k : extraOptions.keys()) {
                newSc.extraOptions[k] = extraOptions[k].toString();
            }
        }
        return newSc;
    }
}

void MainWindow::loadProject(const QString& filepath)
{
    QFile pfile(filepath);
    pfile.open(QIODevice::ReadOnly);
    if (!pfile.isOpen()) {
        QMessageBox::warning(this, "MiniZinc IDE",
                             "Could not open project file");
        return;
    }
    QByteArray jsonData = pfile.readAll();
    QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonData);
    bool jsonError = false;
    if (!jsonDoc.isNull()) {
        QStringList projectFilesRelPath;
        QString basePath = QFileInfo(filepath).absolutePath()+"/";
        QStringList openFiles;
        int openTab = -1;
        if (jsonDoc.isObject()) {
            QJsonObject confObject(jsonDoc.object());
            projectPath = filepath;
            updateRecentProjects(projectPath);
            project.setRoot(ui->projectView, projectSort, projectPath);
            if (confObject["openFiles"].isArray()) {
                QJsonArray openFilesA(confObject["openFiles"].toArray());
                for (auto f : openFilesA) {
                    if (f.isString()) {
                        openFiles.append(f.toString());
                    } else {
                        jsonError = true;
                        goto errorInJsonConfig;
                    }
                }
            }
            openTab = confObject["openTab"].isDouble() ? confObject["openTab"].toDouble() : -1;
            if (confObject["projectFiles"].isArray()) {
                QJsonArray projectFilesA(confObject["projectFiles"].toArray());
                for (auto f : projectFilesA) {
                    if (f.isString()) {
                        projectFilesRelPath.append(f.toString());
                    } else {
                        jsonError = true;
                        goto errorInJsonConfig;
                    }
                }
            }
            // Load project solver configurations
            if (confObject["projectSolverConfigs"].isArray()) {
                QJsonArray solverConfA(confObject["projectSolverConfigs"].toArray());
                for (auto sc : solverConfA) {
                    if (sc.isObject()) {
                        QJsonObject sco(sc.toObject());
                        SolverConfiguration newSc = scFromJson(sco);
                        projectSolverConfigs.push_back(newSc);
                    } else {
                        jsonError = true;
                        goto errorInJsonConfig;
                    }
                }
            }
            project.solverConfigs(projectSolverConfigs,true);
            // Load built-in solver configurations
            if (confObject["builtinSolverConfigs"].isArray()) {
                QJsonArray solverConfA(confObject["builtinSolverConfigs"].toArray());
                for (auto sc : solverConfA) {
                    if (sc.isObject()) {
                        QJsonObject sco(sc.toObject());
                        SolverConfiguration newSc = scFromJson(sco);
                        int updateSc = -1;
                        for (int i=0; i<builtinSolverConfigs.size(); i++) {
                            if (newSc.solverId==builtinSolverConfigs[i].solverId) {
                                updateSc = i;
                                if (newSc.solverVersion==builtinSolverConfigs[i].solverVersion) {
                                    builtinSolverConfigs[i] = newSc;
                                    updateSc = -1;
                                    break;
                                }
                            }
                        }
                        if (updateSc != -1) {
                            builtinSolverConfigs[updateSc] = newSc;
                        }
                    } else {
                        jsonError = true;
                        goto errorInJsonConfig;
                    }
                }
            }
            updateSolverConfigs();
            if (confObject["selectedProjectConfig"].isDouble()) {
                int selected = confObject["selectedProjectConfig"].toDouble();
                if (selected < projectSolverConfigs.size())
                    setCurrentSolverConfig(selected);
            } else {
                if (confObject["selectedBuiltinConfigId"].isString() && confObject["selectedBuiltinConfigVersion"].isString()) {
                    int selectConfig = 0;
                    QString selId = confObject["selectedBuiltinConfigId"].toString();
                    QString selVer = confObject["selectedBuiltinConfigVersion"].toString();
                    for (int i=0; i<builtinSolverConfigs.size(); i++) {
                        if (selId==builtinSolverConfigs[i].solverId) {
                            selectConfig = i;
                            if (selVer==builtinSolverConfigs[i].solverVersion)
                                break;
                        }
                    }
                    setCurrentSolverConfig(projectSolverConfigs.size()+selectConfig);
                } else {
                    jsonError = true;
                }
            }
        } else {
            jsonError = true;
        }
      errorInJsonConfig:
        if (jsonError) {
            QMessageBox::warning(this, "MiniZinc IDE",
                                 "Error in project file");
        } else {
            QStringList missingFiles;
            for (int i=0; i<projectFilesRelPath.size(); i++) {
                QFileInfo fi(basePath+projectFilesRelPath[i]);
                if (fi.exists()) {
                    project.addFile(ui->projectView, projectSort, basePath+projectFilesRelPath[i]);
                } else {
                    missingFiles.append(basePath+projectFilesRelPath[i]);
                }
            }
            if (!missingFiles.empty()) {
                QMessageBox::warning(this, "MiniZinc IDE", "Could not find files in project:\n"+missingFiles.join("\n"));
            }

            for (int i=0; i<openFiles.size(); i++) {
                QFileInfo fi(basePath+openFiles[i]);
                if (fi.exists()) {
                    openFile(basePath+openFiles[i],false);
                }
            }

            project.setModified(false, true);

            IDE::instance()->projects.insert(projectPath, this);
            ui->tabWidget->setCurrentIndex(openTab != -1 ? openTab : ui->tabWidget->currentIndex());
            if (ui->projectExplorerDockWidget->isHidden()) {
                on_actionShow_project_explorer_triggered();
            }

        }
        return;

    } else {
        pfile.reset();
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
        if (version != 101 && version != 102 && version != 103 && version != 104) {
            QMessageBox::warning(this, "MiniZinc IDE",
                                 "Could not open project file (version mismatch)");
            close();
            return;
        }
        in.setVersion(QDataStream::Qt_5_0);

        projectPath = filepath;
        updateRecentProjects(projectPath);
        project.setRoot(ui->projectView, projectSort, projectPath);
        QString basePath;
        if (version==103 || version==104) {
            basePath = QFileInfo(filepath).absolutePath()+"/";
        }

        QStringList openFiles;
        in >> openFiles;
        QString p_s;
        bool p_b;

        int dataFileIndex;

        SolverConfiguration newConf;
        newConf.isBuiltin = false;
        newConf.runSolutionChecker = true;

        in >> p_s; // Used to be additional include path
        in >> dataFileIndex;
        in >> p_b;
        // Ignore, not used any longer
    //    project.haveExtraArgs(p_b, true);
        in >> newConf.additionalData;
        in >> p_b;
        // Ignore, not used any longer
        in >> newConf.additionalCompilerCommandline;
        if (version==104) {
            in >> newConf.clearOutputWindow;
        } else {
            newConf.clearOutputWindow = false;
        }
        in >> newConf.verboseFlattening;
        in >> p_b;
        newConf.optimizationLevel = p_b ? 1 : 0;
        in >> p_s; // Used to be solver name
        in >> newConf.stopAfter;
        in >> newConf.printIntermediate;
        in >> newConf.solvingStats;
        in >> p_b;
        // Ignore
        in >> newConf.solverFlags;
        in >> newConf.nThreads;
        in >> p_b;
        in >> p_s;
        newConf.randomSeed = p_b ? QVariant::fromValue(p_s.toInt()) : QVariant();
        in >> p_b; // used to be whether time limit is checked
        in >> newConf.timeLimit;
        int openTab = -1;
        if (version==102 || version==103 || version==104) {
            in >> newConf.verboseSolving;
            in >> openTab;
        }
        QStringList projectFilesRelPath;
        if (version==103 || version==104) {
            in >> projectFilesRelPath;
        } else {
            projectFilesRelPath = openFiles;
        }
        if ( (version==103 || version==104) && !in.atEnd()) {
            in >> newConf.defaultBehaviour;
        } else {
            newConf.defaultBehaviour = (newConf.stopAfter == 1 && !newConf.printIntermediate);
        }
        if (version==104 && !in.atEnd()) {
            in >> newConf.flatteningStats;
        }
        if (version==104 && !in.atEnd()) {
            in >> newConf.compressSolutionOutput;
        }
        if (version==104 && !in.atEnd()) {
            in >> newConf.outputTiming;
        }
        // create new solver configuration based on projet settings
        newConf.solverId = builtinSolverConfigs[defaultSolverIdx].solverId;
        newConf.solverVersion = builtinSolverConfigs[defaultSolverIdx].solverVersion;
        bool foundConfig = false;
        for (int i=0; i<builtinSolverConfigs.size(); i++) {
            if (builtinSolverConfigs[i]==newConf) {
                setCurrentSolverConfig(i);
                foundConfig = true;
                break;
            }
        }
        if (!foundConfig) {
            newConf.name = "Project solver configuration";
            projectSolverConfigs.push_front(newConf);
            currentSolverConfig = 0;
            project.solverConfigs(projectSolverConfigs,true);
            updateSolverConfigs();
            setCurrentSolverConfig(0);
        }
        QStringList missingFiles;
        for (int i=0; i<projectFilesRelPath.size(); i++) {
            QFileInfo fi(basePath+projectFilesRelPath[i]);
            if (fi.exists()) {
                project.addFile(ui->projectView, projectSort, basePath+projectFilesRelPath[i]);
            } else {
                missingFiles.append(basePath+projectFilesRelPath[i]);
            }
        }
        if (!missingFiles.empty()) {
            QMessageBox::warning(this, "MiniZinc IDE", "Could not find files in project:\n"+missingFiles.join("\n"));
        }

        for (int i=0; i<openFiles.size(); i++) {
            QFileInfo fi(basePath+openFiles[i]);
            if (fi.exists()) {
                openFile(basePath+openFiles[i],false);
            }
        }

        project.setModified(false, true);
        ui->tabWidget->setCurrentIndex(openTab != -1 ? openTab : ui->tabWidget->currentIndex());
        IDE::instance()->projects.insert(projectPath, this);
        if (ui->projectExplorerDockWidget->isHidden()) {
            on_actionShow_project_explorer_triggered();
        }
    }
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
    if (curEditor==NULL)
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
    }
}

void MainWindow::on_actionNext_tab_triggered()
{
    if (ui->tabWidget->currentIndex() < ui->tabWidget->count()-1) {
        ui->tabWidget->setCurrentIndex(ui->tabWidget->currentIndex()+1);
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

void MainWindow::on_conf_timeLimit_valueChanged(int arg1)
{
    if (arg1==0) {
        ui->conf_timeLimit_label->setText("seconds (disabled)");
    } else {
        ui->conf_timeLimit_label->setText("seconds");
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
        ui->outputConsole->document()->setDefaultStyleSheet(".mznnotice { color : green }");
    } else {
        ui->outputConsole->document()->setDefaultStyleSheet(".mznnotice { color : blue }");
    }
}

void MainWindow::on_actionEditSolverConfig_triggered()
{
    if (ui->conf_dock_widget->isHidden()) {
        ui->conf_dock_widget->show();
    } else {
        ui->conf_dock_widget->hide();
    }
}

void MainWindow::on_conf_dock_widget_visibilityChanged(bool visible)
{
    if (visible) {
        ui->actionEditSolverConfig->setText("Hide configuration editor...");
    } else {
        ui->actionEditSolverConfig->setText("Show configuration editor...");
    }
}

void MainWindow::on_conf_check_solutions_toggled(bool)
{
    tabChange(ui->tabWidget->currentIndex());
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
    if (!cursor.hasSelection()) {
        find(true);
        cursor = curEditor->textCursor();
    }
    cursor.beginEditBlock();
    while (cursor.hasSelection()) {
        counter++;
        cursor.insertText(ui->replace->text());
        find(true);
        cursor = curEditor->textCursor();
    }
    cursor.endEditBlock();
    if (counter > 0) {
        ui->not_found->setText(QString().number(counter)+" replaced");
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
