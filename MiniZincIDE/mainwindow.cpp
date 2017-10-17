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
#include <csignal>

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "aboutdialog.h"
#include "codeeditor.h"
#include "fzndoc.h"
#include "finddialog.h"
#include "gotolinedialog.h"
#include "help.h"
#include "paramdialog.h"
#include "checkupdatedialog.h"
#include "moocsubmission.h"

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
#include "rtfexporter.h"
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
    helpWindow = new Help();

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

    QList<QObject*> lst = mwb->children();
    foreach (QObject* mo, lst) {
        if (QMenu* m = qobject_cast<QMenu*>(mo)) {
            if (m->title()=="&File" || m->title()=="Help") {
                QMenu* nm = defaultMenuBar->addMenu(m->title());
                foreach (QAction* a, m->actions()) {
                    if (a->isSeparator()) {
                        nm->addSeparator();
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
    mainWindows.remove(mw);
    delete mw;
#endif

    checkUpdate();
}

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

void IDE::fileModified(const QString &f)
{
    modifiedFiles.insert(f);
    if (activeWindow()!=NULL) {
        modifiedTimer.setSingleShot(true);
        modifiedTimer.start(3000);
    }
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

void IDE::openFile()
{
    QString fileName = QFileDialog::getOpenFileName(NULL, tr("Open File"), getLastPath(), "MiniZinc Files (*.mzn *.dzn *.fzn *.mzp)");
    if (!fileName.isNull()) {
        setLastPath(QFileInfo(fileName).absolutePath()+fileDialogSuffix);
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
    helpWindow->show();
    helpWindow->raise();
    helpWindow->activateWindow();
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
    curHtmlWindow(NULL),
    process(NULL),
    outputProcess(NULL),
    tmpDir(NULL),
    saveBeforeRunning(false),
    project(ui),
    outputBuffer(NULL),
    processRunning(false)
{
    init(project);
}

MainWindow::MainWindow(const QStringList& files) :
    ui(new Ui::MainWindow),
    curEditor(NULL),
    curHtmlWindow(NULL),
    process(NULL),
    outputProcess(NULL),
    tmpDir(NULL),
    saveBeforeRunning(false),
    project(ui),
    outputBuffer(NULL),
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
    ui->toolBar->insertWidget(ui->actionShow_project_explorer, toolBarSpacer);

    newFileCounter = 1;

    findDialog = new FindDialog(this);
    findDialog->setModal(false);

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
    statusLabel = new QLabel("");
    ui->statusbar->addPermanentWidget(statusLabel);
    ui->statusbar->showMessage("Ready.");
    ui->actionStop->setEnabled(false);
    QTabBar* tb = ui->tabWidget->findChild<QTabBar*>();
    tb->setTabButton(0, QTabBar::RightSide, 0);
    tabChange(0);
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

    Solver g12fd("G12 fd","flatzinc","-Gg12_fd","",true,false,true);
    Solver g12lazyfd("G12 lazyfd","flatzinc","-Gg12_lazyfd","-b lazy",true,false,true);
    Solver g12mip("G12 MIP","flatzinc","-Glinear","-b mip",true,false,true);
#ifdef MINIZINC_IDE_BUNDLED
    Solver gecode("Gecode (bundled)","fzn-gecode","-Ggecode","",true,false,true);
#ifdef Q_OS_WIN
    Solver gecodeGist("Gecode (Gist, bundled)","fzn-gecode-gist.bat","-Ggecode","",true,true,true);
#else
    Solver gecodeGist("Gecode (Gist, bundled)","fzn-gecode-gist","-Ggecode","",true,true,true);
#endif
    Solver chuffed("Chuffed (bundled)","fzn-chuffed","-Gchuffed","",true,false,true);
    Solver cbc("COIN-OR CBC (bundled)","mzn-cbc","-Glinear","",true,false,true);
    Solver gurobi("Gurobi (bundled)","mzn-gurobi","-Glinear","",true,false,true);
#endif

    int nsolvers = settings.beginReadArray("solvers");
#ifdef MINIZINC_IDE_BUNDLED
    solvers.append(gecode);
    solvers.append(gecodeGist);
    solvers.append(chuffed);
    solvers.append(cbc);
    solvers.append(gurobi);
#endif
    solvers.append(g12fd);
    solvers.append(g12lazyfd);
    solvers.append(g12mip);
    IDE::instance()->stats.solvers.clear();
    for (int i=0; i<nsolvers; i++) {
        settings.setArrayIndex(i);
        if (settings.value("builtin").toBool())
            continue;
        Solver solver;
        solver.name = settings.value("name").toString();
        solver.executable = settings.value("executable").toString();
        solver.mznlib = settings.value("mznlib").toString();
        solver.backend = settings.value("backend").toString();
        solver.builtin = settings.value("builtin").toBool();
        solver.detach = settings.value("detach",false).toBool();
        solver.needs_mzn2fzn= settings.value("needs_mzn2fzn",true).toBool();
        IDE::instance()->stats.solvers.append(solver.name);
        solvers.append(solver);
    }
    settings.endArray();
    settings.beginGroup("minizinc");
    mznDistribPath = settings.value("mznpath","").toString();
    defaultSolver = settings.value("defaultSolver","").toString();
    settings.endGroup();
    checkMznPath();
    for (int i=0; i<solvers.size(); i++)
        ui->conf_solver->addItem(solvers[i].name,i);
    ui->conf_solver->addItem("Add new solver...");
    if (!defaultSolver.isEmpty())
        ui->conf_solver->setCurrentText(defaultSolver);

    connect(QApplication::clipboard(), SIGNAL(dataChanged()), this, SLOT(onClipboardChanged()));

    projectSort = new QSortFilterProxyModel(this);
    projectSort->setDynamicSortFilter(true);
    projectSort->setSourceModel(&project);
    projectSort->setSortRole(Qt::UserRole);
    ui->projectView->setModel(projectSort);
    ui->projectView->sortByColumn(0, Qt::AscendingOrder);
    ui->projectView->setEditTriggers(QAbstractItemView::EditKeyPressed);
    ui->projectExplorerDockWidget->hide();
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

    connect(ui->conf_data_file, SIGNAL(currentIndexChanged(int)), &project, SLOT(currentDataFileIndex(int)));
    connect(ui->conf_have_cmd_params, SIGNAL(toggled(bool)), &project, SLOT(haveExtraArgs(bool)));
    connect(ui->conf_cmd_params, SIGNAL(textEdited(QString)), &project, SLOT(extraArgs(QString)));
    connect(ui->conf_have_mzn2fzn_params, SIGNAL(toggled(bool)), &project, SLOT(haveExtraMzn2FznArgs(bool)));
    connect(ui->conf_mzn2fzn_params, SIGNAL(textEdited(QString)), &project, SLOT(extraMzn2FznArgs(QString)));
    connect(ui->conf_verbose, SIGNAL(toggled(bool)), &project, SLOT(mzn2fznVerbose(bool)));
    connect(ui->autoclear_output, SIGNAL(toggled(bool)), &project, SLOT(autoClearOutput(bool)));
    connect(ui->conf_optimize, SIGNAL(toggled(bool)), &project, SLOT(mzn2fznOptimize(bool)));
    connect(ui->conf_solver, SIGNAL(currentIndexChanged(QString)), &project, SLOT(currentSolver(QString)));
    connect(ui->conf_nsol, SIGNAL(valueChanged(int)), &project, SLOT(n_solutions(int)));
    connect(ui->conf_printall, SIGNAL(toggled(bool)), &project, SLOT(printAll(bool)));
    connect(ui->conf_stats, SIGNAL(toggled(bool)), &project, SLOT(printStats(bool)));
    connect(ui->conf_have_solverFlags, SIGNAL(toggled(bool)), &project, SLOT(haveSolverFlags(bool)));
    connect(ui->conf_nthreads, SIGNAL(valueChanged(int)), &project, SLOT(n_threads(int)));
    connect(ui->conf_have_seed, SIGNAL(toggled(bool)), &project, SLOT(haveSeed(bool)));
    connect(ui->conf_seed, SIGNAL(textEdited(QString)), &project, SLOT(seed(QString)));
    connect(ui->conf_timeLimit, SIGNAL(valueChanged(int)), &project, SLOT(timeLimit(int)));
    connect(ui->conf_solver_verbose, SIGNAL(toggled(bool)), &project, SLOT(solverVerbose(bool)));

    if (!projectFile.isEmpty()) {
        loadProject(projectFile);
        setLastPath(QFileInfo(projectFile).absolutePath()+fileDialogSuffix);
    } else {
        on_actionNewModel_file_triggered();
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
    setupDznMenu();
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
            if (ui->tabWidget->widget(i) != ui->configuration) {
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
    }
    project.removeFile(projectSelectedFile);
    setupDznMenu();
}

void MainWindow::onActionProjectRename_triggered()
{
    ui->projectView->edit(ui->projectView->currentIndex());
}

void MainWindow::onActionProjectRunWith_triggered()
{
    ui->conf_data_file->setCurrentIndex(ui->conf_data_file->findText(projectSelectedFile));
    on_actionRun_triggered();
}

void MainWindow::activateFileInProject(const QModelIndex &proxyIndex)
{
    QModelIndex index = projectSort->mapToSource(proxyIndex);
    if (project.isProjectFile(index)) {
        ui->tabWidget->setCurrentWidget(ui->configuration);
    } else {
        QString fileName = project.fileAtIndex(index);
        if (!fileName.isEmpty()) {
            bool foundFile = false;
            for (int i=0; i<ui->tabWidget->count(); i++) {
                if (ui->tabWidget->widget(i) != ui->configuration) {
                    CodeEditor* ce = static_cast<CodeEditor*>(ui->tabWidget->widget(i));
                    if (ce->filepath == fileName) {
                        ui->tabWidget->setCurrentIndex(i);
                        foundFile = true;
                        break;
                    }
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
        cleanupProcesses[i]->kill();
        cleanupProcesses[i]->waitForFinished();
        delete cleanupProcesses[i];
    }
    if (process) {
        process->kill();
        process->waitForFinished();
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

void MainWindow::createEditor(const QString& path, bool openAsModified, bool isNewFile, bool readOnly) {
    QTextDocument* doc = NULL;
    bool large = false;
    QString fileContents;
    QString absPath = QFileInfo(path).canonicalFilePath();
    if (isNewFile) {
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
        if (!isNewFile && ui->tabWidget->count()==2) {
            CodeEditor* ce =
                    static_cast<CodeEditor*>(ui->tabWidget->widget(0)==ui->configuration ?
                                                 ui->tabWidget->widget(1) : ui->tabWidget->widget(0));
            if (ce->filepath == "" && !ce->document()->isModified()) {
                closeTab = ui->tabWidget->widget(0)==ui->configuration ? 1 : 0;
            }
        }
        CodeEditor* ce = new CodeEditor(doc,absPath,isNewFile,large,editorFont,darkMode,ui->tabWidget,this);
        if (readOnly || ce->filename == "_coursera")
            ce->setReadOnly(true);
        int tab = ui->tabWidget->addTab(ce, ce->filename);
        ui->tabWidget->setCurrentIndex(tab);
        curEditor->setFocus();
        if (openAsModified) {
            curEditor->filepath = "";
            curEditor->document()->setPlainText(fileContents);
            curEditor->document()->setModified(true);
            tabChange(ui->tabWidget->currentIndex());
        } else if (doc) {
            project.addFile(ui->projectView, projectSort, absPath);
            IDE::instance()->registerEditor(absPath,curEditor);
        }
        if (closeTab >= 0)
            tabCloseRequest(closeTab);
        setupDznMenu();
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

void MainWindow::openFile(const QString &path, bool openAsModified)
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
            createEditor(fileName, openAsModified, false);
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
    setupDznMenu();
    if (!ce->filepath.isEmpty())
        IDE::instance()->removeEditor(ce->filepath,ce);
    delete ce;
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
        process->kill();
    }
    for (int i=0; i<ui->tabWidget->count(); i++) {
        if (ui->tabWidget->widget(i) != ui->configuration) {
            CodeEditor* ce = static_cast<CodeEditor*>(ui->tabWidget->widget(i));
            ce->setDocument(NULL);
            ce->filepath = "";
            if (ce->filepath != "")
                IDE::instance()->removeEditor(ce->filepath,ce);
        }
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
            connect(curEditor->document(), SIGNAL(undoAvailable(bool)),
                    ui->actionUndo, SLOT(setEnabled(bool)));
            connect(curEditor->document(), SIGNAL(redoAvailable(bool)),
                    ui->actionRedo, SLOT(setEnabled(bool)));
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
            }
            ui->actionSave->setEnabled(true);
            ui->actionSave_as->setEnabled(true);
            ui->actionSelect_All->setEnabled(true);
            ui->actionUndo->setEnabled(curEditor->document()->isUndoAvailable());
            ui->actionRedo->setEnabled(curEditor->document()->isRedoAvailable());
            updateUiProcessRunning(processRunning);

            findDialog->setEditor(curEditor);
            ui->actionFind->setEnabled(true);
            ui->actionFind_next->setEnabled(true);
            ui->actionFind_previous->setEnabled(true);
            ui->actionReplace->setEnabled(true);
            ui->actionShift_left->setEnabled(true);
            ui->actionShift_right->setEnabled(true);
            curEditor->setFocus();
        } else {
            curEditor = NULL;
            setWindowModified(project.isModified());
            connect(&project, SIGNAL(modificationChanged(bool)),
                    this, SLOT(setWindowModified(bool)));
            ui->actionClose->setEnabled(false);
            ui->actionSave->setEnabled(false);
            ui->actionSave_as->setEnabled(false);
            ui->actionCut->setEnabled(false);
            ui->actionCopy->setEnabled(false);
            ui->actionPaste->setEnabled(false);
            ui->actionSelect_All->setEnabled(false);
            ui->actionUndo->setEnabled(false);
            ui->actionRedo->setEnabled(false);
            fakeRunAction->setEnabled(true);
            fakeCompileAction->setEnabled(true);
            ui->actionRun->setEnabled(false);
            ui->actionCompile->setEnabled(false);
            ui->actionFind->setEnabled(false);
            ui->actionFind_next->setEnabled(false);
            ui->actionFind_previous->setEnabled(false);
            ui->actionReplace->setEnabled(false);
            ui->actionShift_left->setEnabled(false);
            ui->actionShift_right->setEnabled(false);
            findDialog->close();
            setWindowFilePath(projectPath);
            QString p;
            if (projectPath.isEmpty()) {
                p = "Untitled Project";
            } else {
                QFileInfo fi(projectPath);
                p = "Project: "+fi.baseName();
            }
            setWindowTitle(p+"[*]");
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

QStringList MainWindow::parseConf(bool compileOnly, bool useDataFile)
{
    QStringList ret;
    if (compileOnly && !project.mzn2fznOptimize())
        ret << "--no-optimize";
    if (compileOnly && project.mzn2fznVerbose())
        ret << "-v";
    if (compileOnly && project.mzn2fznPrintStats())
        ret << "-s";
    if (compileOnly && project.haveExtraArgs() &&
        !project.extraArgs().isEmpty())
        ret << "-D" << project.extraArgs();
    if (compileOnly && project.haveExtraMzn2FznArgs() &&
        !project.extraMzn2FznArgs().isEmpty()) {
        QStringList compilerArgs =
                project.extraMzn2FznArgs().split(" ", QString::SkipEmptyParts);
        ret << compilerArgs;
    }
    if (compileOnly && useDataFile && project.currentDataFile()!="None")
        ret << "-d" << project.currentDataFile();
    bool isOptimisationProblem = true;
    {
        QFile fznFile(currentFznTarget);
        if (fznFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            int seekSize = strlen("satisfy;\n\n");
            if (fznFile.size() >= seekSize) {
                fznFile.seek(fznFile.size()-seekSize);
                QString line = fznFile.readLine();
                if (line.contains("satisfy;"))
                    isOptimisationProblem = false;
            }
        }
    }

    if (!compileOnly) {
        if (project.defaultBehaviour()) {
            if (isOptimisationProblem)
                ret << "-a";
        } else {
            if (isOptimisationProblem) {
                if (project.printAll())
                    ret << "-a";
            } else {
                if (project.n_solutions() == 0)
                    ret << "-a";
                else if (project.n_solutions() > 1)
                    ret << "-n" << QString::number(project.n_solutions());
            }
        }
    }

    if (!compileOnly && project.printStats())
        ret << "-s";
    if (!compileOnly && project.n_threads() > 1)
        ret << "-p" << QString::number(project.n_threads());
    if (!compileOnly && project.haveSeed())
        ret << "-r" << project.seed();
    if (!compileOnly && project.haveSolverFlags()) {
        QStringList solverArgs =
                project.solverFlags().split(" ", QString::SkipEmptyParts);
        ret << solverArgs;
    }
    Solver s = solvers[ui->conf_solver->itemData(ui->conf_solver->currentIndex()).toInt()];
    if (compileOnly && !s.mznlib.isEmpty())
        ret << s.mznlib;
    return ret;
}

void MainWindow::setupDznMenu()
{
    QString curText = ui->conf_data_file->currentText();
    ui->conf_data_file->clear();
    ui->conf_data_file->addItem("None");
    QStringList dataFiles = project.dataFiles();
    for (int i=0; i<dataFiles.size(); i++) {
        ui->conf_data_file->addItem(dataFiles[i]);
    }
    ui->conf_data_file->addItem("Add data file to project...");
    if (curText != "Add data file to project...")
        ui->conf_data_file->setCurrentText(curText);
    else
        ui->conf_data_file->setCurrentIndex(0);
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
    QString additionalDataFile;
    if (exitcode!=0) {
        checkArgsOutput();
        compileErrors = compileErrors.simplified();
        QRegExp undefined("symbol error: variable `([a-zA-Z][a-zA-Z0-9_]*)' must be defined");
        undefined.setMinimal(true);
        int pos = 0;
        QStringList undefinedArgs;
        while ( (pos = undefined.indexIn(compileErrors,pos)) != -1 ) {
            undefinedArgs << undefined.cap(1);
            pos += undefined.matchedLength();
        }
        if (undefinedArgs.size() > 0) {
            QStringList params;
            paramDialog->getParams(undefinedArgs, project.dataFiles(), params, additionalDataFile);
            if (additionalDataFile.isEmpty()) {
                if (params.size()==0) {
                    procFinished(0,false);
                    return;
                }
                for (int i=0; i<undefinedArgs.size(); i++) {
                    if (params[i].isEmpty()) {
                        QMessageBox::critical(this, "Undefined parameter","The parameter `"+undefinedArgs[i]+"' is undefined.");
                        procFinished(0);
                        return;
                    }
                    additionalCmdlineParams += undefinedArgs[i]+"="+params[i]+"; ";
                }
            }
        }
    }
    compileAndRun(curEditor->filepath, additionalCmdlineParams, additionalDataFile);
}

void MainWindow::checkArgs(QString filepath)
{
    if (mzn2fzn_executable=="") {
        int ret = QMessageBox::warning(this,"MiniZinc IDE","Could not find the mzn2fzn executable.\nDo you want to open the solver settings dialog?",
                                       QMessageBox::Ok | QMessageBox::Cancel);
        if (ret == QMessageBox::Ok)
            on_actionManage_solvers_triggered();
        return;
    }
    process = new MznProcess(this);
    processName = mzn2fzn_executable;
    processWasStopped = false;
    process->setWorkingDirectory(QFileInfo(filepath).absolutePath());
    process->setProcessChannelMode(QProcess::MergedChannels);
    connect(process, SIGNAL(readyRead()), this, SLOT(checkArgsOutput()));
    connect(process, SIGNAL(finished(int,QProcess::ExitStatus)), this, SLOT(checkArgsFinished(int,QProcess::ExitStatus)));
    connect(process, SIGNAL(error(QProcess::ProcessError)),
            this, SLOT(checkArgsError(QProcess::ProcessError)));

    QStringList args = parseConf(true, true);
    args << "--instance-check-only" << "--output-to-stdout";
    args << filepath;
    compileErrors = "";
    elapsedTime.start();
    process->start(mzn2fzn_executable,args,getMznDistribPath());
}

void MainWindow::on_actionRun_triggered()
{
    if (mzn2fzn_executable=="") {
        int ret = QMessageBox::warning(this,"MiniZinc IDE","Could not find the mzn2fzn executable.\nDo you want to open the solver settings dialog?",
                                       QMessageBox::Ok | QMessageBox::Cancel);
        if (ret == QMessageBox::Ok)
            on_actionManage_solvers_triggered();
        return;
    }
    if (curEditor && curEditor->filepath!="") {
        if (curEditor->document()->isModified()) {
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
        if (curEditor->document()->isModified())
            return;
        if (project.autoClearOutput()) {
            on_actionClear_output_triggered();
        }
        updateUiProcessRunning(true);
        on_actionSplit_triggered();
        IDE::instance()->stats.modelsRun++;
        if (curEditor->filepath.endsWith(".fzn")) {
            currentFznTarget = curEditor->filepath;
            runSolns2Out = false;
            runCompiledFzn(0,QProcess::NormalExit);
        } else {
            compileOnly = false;
            checkArgs(curEditor->filepath);
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
    if (project.timeLimit() > 0) {
        timeLimit += " / ";
        int tl_hours = project.timeLimit() / 3600;
        int tl_minutes = (project.timeLimit() % 3600) / 60;
        int tl_seconds = project.timeLimit() % 60;
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
                } else
                    if (l.startsWith("%%%mzn-json-end")) {
                    curJSONHandler++;
                    inJSONHandler = false;
                } else {
                    JSONOutput[curJSONHandler].append(l);
                }
            } else {
                QRegExp pattern("^(?:%%%(top|bottom))?%%%mzn-json:(.*)");
                if (pattern.exactMatch(l.trimmed())) {
                    inJSONHandler = true;
                    QStringList sl;
                    sl.append(pattern.capturedTexts()[2]);
                    if (pattern.capturedTexts()[1].isEmpty()) {
                        sl.append("top");
                    } else {
                        sl.append(pattern.capturedTexts()[1]);
                    }
                    JSONOutput.append(sl);
                } else {
                    if (l.trimmed() == "----------") {
                        solutionCount++;
                        if ( solutionCount > solutionLimit || !hiddenSolutions.isEmpty()) {
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
                    } else if (curHtmlWindow && l.trimmed() == "==========") {
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
                            if (solutionCount == solutionLimit) {
                                addOutput("<div style='color:blue;'>[ "+QString().number(solutionLimit)+" more solutions ]</div>");
                                solutionCount = 0;
                                solutionLimit *= 2;
                            }
                        } else {
                            addOutput(l,false);
                        }
                        if (!hiddenSolutions.isEmpty() && l.trimmed() == "==========") {
                            if (solutionCount!=solutionLimit && solutionCount > 1) {
                                addOutput("<div style='color:blue;'>[ "+QString().number(solutionCount-1)+" more solutions ]</div>");
                            }
                            for (int i=hiddenSolutions.size()-2; i<hiddenSolutions.size(); i++) {
                                addOutput(hiddenSolutions[i], false);
                            }
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
            QRegExp errexp("^(.*):([0-9]+):\\s*$");
            if (errexp.indexIn(l) != -1) {
                QString errFile = errexp.cap(1).trimmed();
                QUrl url = QUrl::fromLocalFile(errFile);
                url.setQuery("line="+errexp.cap(2));
                url.setScheme("err");
                IDE::instance()->stats.errorsShown++;
                addOutput("<a style='color:red' href='"+url.toString()+"'>"+errexp.cap(1)+":"+errexp.cap(2)+":</a>");
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
    if (curHtmlWindow==NULL) {
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
        curHtmlWindow = new HTMLWindow(specs, this);
        connect(curHtmlWindow, SIGNAL(closeWindow()), this, SLOT(closeHTMLWindow()));
        curHtmlWindow->show();
    }
    for (int i=0; i<JSONOutput.size(); i++) {
        JSONOutput[i].pop_front();
        JSONOutput[i].pop_front();
        curHtmlWindow->addSolution(i, JSONOutput[i].join(' '));
    }
}

void MainWindow::finishJSONViewer(void)
{
    if (curHtmlWindow) {
        curHtmlWindow->finish(elapsedTime.elapsed());
    }
}

void MainWindow::compileAndRun(const QString& modelPath, const QString& additionalCmdlineParams, const QString& additionalDataFile)
{
    process = new MznProcess(this);
    processName = mzn2fzn_executable;

    bool standalone = false;
    if (!compileOnly) {
        // Check if we need to run a stand-alone solver (no mzn2fzn or solns2out)
        Solver s = solvers[ui->conf_solver->itemData(ui->conf_solver->currentIndex()).toInt()];
        if (!s.needs_mzn2fzn) {
            standalone = true;
            processName = s.executable;
        }
    }

    curFilePath = modelPath;
    processWasStopped = false;
    runSolns2Out = true;
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

    QStringList args = parseConf(true, additionalDataFile.isEmpty());
    if (!additionalCmdlineParams.isEmpty()) {
        args << "-D" << additionalCmdlineParams;
    }
    if (!additionalDataFile.isEmpty()) {
        args << "-d" << additionalDataFile;
    }

    if (standalone) {
        QStringList runArgs = parseConf(false,true);
        args << runArgs;
    }

    tmpDir = new QTemporaryDir;
    if (!tmpDir->isValid()) {
        QMessageBox::critical(this, "MiniZinc IDE", "Could not create temporary directory for compilation.");
        procFinished(0);
    } else {
        QFileInfo fi(modelPath);
        if (!standalone) {
            currentFznTarget = tmpDir->path()+"/"+fi.baseName()+".fzn";
            args << "-o" << currentFznTarget;
            args << "--output-ozn-to-file" << tmpDir->path()+"/"+fi.baseName()+".ozn";
        }
        args << modelPath;
        QString compiling = (standalone ? "Running " : "Compiling ") + fi.fileName();
        if (project.currentDataFile()!="None") {
            compiling += " with data ";
            QFileInfo fi(project.currentDataFile());
            compiling += fi.fileName();
        }
        if (!additionalDataFile.isEmpty()) {
            compiling += ", with additional data ";
            QFileInfo fi(additionalDataFile);
            compiling += fi.fileName();
        }
        if (!additionalCmdlineParams.isEmpty()) {
            compiling += ", additional arguments " + additionalCmdlineParams;
        }
        addOutput("<div style='color:blue;'>"+compiling+"</div>");
        time = 0;
        timer->start(500);
        elapsedTime.start();
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
    project.timeLimit(timeout, true);
    updateUiProcessRunning(true);
    on_actionSplit_triggered();
    compileAndRun(modelFilePath,"",dataFilePath);
    return true;
}

void MainWindow::closeHTMLWindow(void)
{
    on_actionStop_triggered();
    delete curHtmlWindow;
    curHtmlWindow = NULL;
}

void MainWindow::selectJSONSolution(HTMLPage* source, int n)
{
    if (curHtmlWindow!=NULL) {
        curHtmlWindow->selectSolution(source,n);
    }
}

void MainWindow::outputProcFinished(int, bool showTime) {
    readOutput();
    updateUiProcessRunning(false);
    timer->stop();
    QString elapsedTime = setElapsedTime();
    ui->statusbar->showMessage("Ready.");
    process = NULL;
    outputProcess = NULL;
    finishJSONViewer();
    inJSONHandler = false;
    JSONOutput.clear();
    if (showTime) {
        addOutput("<div style='color:blue;'>Finished in "+elapsedTime+"</div>");
    }
    delete tmpDir;
    tmpDir = NULL;
    outputBuffer = NULL;
    compileErrors = "";
    emit(finished());
}

void MainWindow::procFinished(int, bool showTime) {
    if (outputProcess && outputProcess->state()!=QProcess::NotRunning) {
        connect(outputProcess, SIGNAL(finished(int)), this, SLOT(outputProcFinished(int)));
        outputProcess->closeWriteChannel();
        return;
    }
    updateUiProcessRunning(false);
    timer->stop();
    QString elapsedTime = setElapsedTime();
    ui->statusbar->showMessage("Ready.");
    process = NULL;
    outputProcess = NULL;
    finishJSONViewer();
    inJSONHandler = false;
    JSONOutput.clear();
    if (showTime) {
        addOutput("<div style='color:blue;'>Finished in "+elapsedTime+"</div>");
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
    procFinished(1);
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
    procFinished(1);
    if (e==QProcess::FailedToStart) {
        QMessageBox::critical(this, "MiniZinc IDE", "Failed to start '"+processName+"'. Check your path settings.");
    } else {
        QMessageBox::critical(this, "MiniZinc IDE", "Unknown error while executing the MiniZinc interpreter `"+processName+"': error code "+QString().number(e));
    }
}

void MainWindow::outputProcError(QProcess::ProcessError e) {
    procFinished(1);
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

        filepath = QFileDialog::getSaveFileName(this,"Save file",dialogPath,"MiniZinc files (*.mzn *.dzn *.fzn)");
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
                    setupDznMenu();
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
        if (ui->tabWidget->widget(i) != ui->configuration) {
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
        setupDznMenu();
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

#ifdef Q_OS_WIN
        AttachConsole(process->pid()->dwProcessId);
        SetConsoleCtrlHandler(NULL, TRUE);
        GenerateConsoleCtrlEvent(CTRL_C_EVENT, 0);
#else
        ::kill(process->pid(), SIGINT);
#endif
        if (!process->waitForFinished(500)) {
            if (process->state() != QProcess::NotRunning) {
                process->kill();
                process->waitForFinished();
            }
        }
        delete process;
        process = NULL;
        addOutput("<div style='color:blue;'>Stopped.</div>");
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
        openFile(currentFznTarget, true);
    }
    procFinished(exitcode);
}

void MainWindow::runCompiledFzn(int exitcode, QProcess::ExitStatus exitstatus)
{
    if (processWasStopped)
        return;
    if (exitcode==0 && exitstatus==QProcess::NormalExit) {
        readOutput();
        QStringList args = parseConf(false,true);
        Solver s = solvers[ui->conf_solver->itemData(ui->conf_solver->currentIndex()).toInt()];        
        if (!s.backend.isEmpty())
            args << s.backend.split(" ",QString::SkipEmptyParts);

        args << currentFznTarget;

        if (s.detach) {
            addOutput("<div style='color:blue;'>Running "+curEditor->filename+" (detached)</div>");

            MznProcess* detached_process = new MznProcess(this);
            detached_process->setWorkingDirectory(QFileInfo(curEditor->filepath).absolutePath());

            QString executable = s.executable;
            if (project.solverVerbose()) {
                addOutput("<div style='color:blue;'>Command line:</div>");
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
            solutionCount = 0;
            solutionLimit = project.defaultBehaviour() ? 100 : project.n_compress_solutions();
            hiddenSolutions.clear();
            if (runSolns2Out) {
                outputProcess = new MznProcess(this);
                inJSONHandler = false;
                curJSONHandler = 0;
                JSONOutput.clear();
                if (curHtmlWindow) {
                    disconnect(curHtmlWindow, SIGNAL(closeWindow()), this, SLOT(closeHTMLWindow()));
                    curHtmlWindow->setAttribute(Qt::WA_DeleteOnClose, true);
                    curHtmlWindow = NULL;
                }
                hadNonJSONOutput = false;
                outputProcess->setWorkingDirectory(QFileInfo(curFilePath).absolutePath());
                connect(outputProcess, SIGNAL(readyReadStandardOutput()), this, SLOT(readOutput()));
                connect(outputProcess, SIGNAL(readyReadStandardError()), this, SLOT(readOutput()));
                connect(outputProcess, SIGNAL(error(QProcess::ProcessError)),
                        this, SLOT(outputProcError(QProcess::ProcessError)));
            }
            process = new MznProcess(this);
            processName = s.executable;
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

            if (project.timeLimit() != 0) {
                int timeout = project.timeLimit();
                solverTimeout->start(timeout*1000);
            }

            elapsedTime.start();
            addOutput("<div style='color:blue;'>Running "+QFileInfo(curFilePath).fileName()+"</div>");
            QString executable = s.executable;
            if (project.solverVerbose()) {
                addOutput("<div style='color:blue;'>Command line:</div>");
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
                outargs << currentFznTarget.left(currentFznTarget.length()-4)+".ozn";
                outputProcess->start("solns2out",outargs,getMznDistribPath());
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
    if (mzn2fzn_executable=="") {
        int ret = QMessageBox::warning(this,"MiniZinc IDE","Could not find the mzn2fzn executable.\nDo you want to open the solver settings dialog?",
                                       QMessageBox::Ok | QMessageBox::Cancel);
        if (ret == QMessageBox::Ok)
            on_actionManage_solvers_triggered();
        return;
    }
    if (curEditor && curEditor->filepath!="") {
        if (curEditor->document()->isModified()) {
            if (!saveBeforeRunning) {
                QMessageBox msgBox;
                msgBox.setText("The model has been modified.");
                msgBox.setInformativeText("Do you want to save it before compiling?");
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
        updateUiProcessRunning(true);

        compileOnly = true;
        checkArgs(curEditor->filepath);
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

    ui->outputConsole->setFont(font);
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

void MainWindow::on_actionAbout_MiniZinc_IDE_triggered()
{
    AboutDialog(IDE::instance()->applicationVersion()).exec();
}

void MainWindow::errorClicked(const QUrl & anUrl)
{
    QUrl url = anUrl;
    QString query = url.query();
    url.setQuery("");
    url.setScheme("file");
    QFileInfo urlinfo(url.toLocalFile());
    IDE::instance()->stats.errorsClicked++;
    for (int i=0; i<ui->tabWidget->count(); i++) {
        if (ui->tabWidget->widget(i) != ui->configuration) {
            CodeEditor* ce = static_cast<CodeEditor*>(ui->tabWidget->widget(i));
            QFileInfo ceinfo(ce->filepath);
            if (ceinfo.canonicalFilePath() == urlinfo.canonicalFilePath()) {
                QRegExp re_line("line=([0-9]+)");
                if (re_line.indexIn(query) != -1) {
                    bool ok;
                    int line = re_line.cap(1).toInt(&ok);
                    if (ok) {
                        QTextBlock block = ce->document()->findBlockByNumber(line-1);
                        if (block.isValid()) {
                            QTextCursor cursor = ce->textCursor();
                            cursor.setPosition(block.position());
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
}

void MainWindow::on_actionManage_solvers_triggered(bool addNew)
{
    QSettings settings;
    settings.beginGroup("ide");
    bool checkUpdates = settings.value("checkforupdates21",false).toBool();
    settings.endGroup();

    SolverDialog sd(solvers,defaultSolver,addNew,mznDistribPath);
    sd.exec();
    defaultSolver = sd.def();
    mznDistribPath = sd.mznPath();
    if (!mznDistribPath.isEmpty() && ! (mznDistribPath.endsWith("/") || mznDistribPath.endsWith("\\")))
        mznDistribPath += "/";
    checkMznPath();
    QString curSelection = ui->conf_solver->currentText();
    ui->conf_solver->clear();
    for (int i=0; i<solvers.size(); i++)
        ui->conf_solver->addItem(solvers[i].name,i);
    ui->conf_solver->addItem("Add new solver...",solvers.size());
    int idx = ui->conf_solver->findText(curSelection);
    if (!addNew && idx!=-1)
        ui->conf_solver->setCurrentIndex(idx);
    else
        ui->conf_solver->setCurrentText(defaultSolver);

    settings.beginGroup("ide");
    if (!checkUpdates && settings.value("checkforupdates21",false).toBool()) {
        settings.setValue("lastCheck21",QDate::currentDate().addDays(-2).toString());
        IDE::instance()->checkUpdate();
    }
    settings.endGroup();

    settings.beginGroup("minizinc");
    settings.setValue("mznpath",mznDistribPath);
    settings.setValue("defaultSolver",defaultSolver);
    settings.endGroup();

    settings.beginWriteArray("solvers");
    QStringList solvers_list;
    for (int i=0; i<solvers.size(); i++) {
        if (!solvers[i].builtin)
            solvers_list.append(solvers[i].name);
        settings.setArrayIndex(i);
        settings.setValue("name",solvers[i].name);
        settings.setValue("executable",solvers[i].executable);
        settings.setValue("mznlib",solvers[i].mznlib);
        settings.setValue("backend",solvers[i].backend);
        settings.setValue("builtin",solvers[i].builtin);
        settings.setValue("detach",solvers[i].detach);
        settings.setValue("needs_mzn2fzn",solvers[i].needs_mzn2fzn);
    }
    IDE::instance()->stats.solvers = solvers_list;
    settings.endArray();
}

void MainWindow::on_actionFind_triggered()
{
    findDialog->raise();
    findDialog->show();
    findDialog->activateWindow();
}

void MainWindow::on_actionReplace_triggered()
{
    findDialog->raise();
    findDialog->show();
    findDialog->activateWindow();
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
    SolverDialog::checkMzn2fznExecutable(mznDistribPath,mzn2fzn_executable,ignoreVersionString);

    if (mzn2fzn_executable.isEmpty()) {
        int ret = QMessageBox::warning(this,"MiniZinc IDE","Could not find the mzn2fzn executable.\nDo you want to open the solver settings dialog?",
                                       QMessageBox::Ok | QMessageBox::Cancel);
        if (ret == QMessageBox::Ok)
            on_actionManage_solvers_triggered();
        return;
    }
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
    if (ui->tabWidget->count() == 1) {
        return project.isUndefined();
    }
    if (ui->tabWidget->count() != 2)
        return false;
    CodeEditor* ce =
            static_cast<CodeEditor*>(ui->tabWidget->widget(0)==ui->configuration ?
                                         ui->tabWidget->widget(1) : ui->tabWidget->widget(0));
    return ce->filepath == "" && !ce->document()->isModified() && !project.isModified();
}

void MainWindow::openProject(const QString& fileName)
{
    if (!fileName.isEmpty()) {
        IDE::PMap& pmap = IDE::instance()->projects;
        IDE::PMap::iterator it = pmap.find(fileName);
        if (it==pmap.end()) {
            if (isEmptyProject()) {
                int closeTab = ui->tabWidget->count()==2 ? (ui->tabWidget->widget(0)==ui->configuration ? 1 : 0) : -1;
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
        IDE::instance()->recentProjects.removeAll(p);
        IDE::instance()->recentProjects.insert(0,p);
        while (IDE::instance()->recentProjects.size() > 7)
            IDE::instance()->recentProjects.pop_back();
    }
    ui->menuRecent_Projects->clear();
    for (int i=0; i<IDE::instance()->recentProjects.size(); i++) {
        ui->menuRecent_Projects->addAction(IDE::instance()->recentProjects[i]);
    }
    ui->menuRecent_Projects->addSeparator();
    ui->menuRecent_Projects->addAction("Clear Menu");
}
void MainWindow::updateRecentFiles(const QString& p) {
    if (!p.isEmpty()) {
        IDE::instance()->recentFiles.removeAll(p);
        IDE::instance()->recentFiles.insert(0,p);
        while (IDE::instance()->recentFiles.size() > 7)
            IDE::instance()->recentFiles.pop_back();
    }
    ui->menuRecent_Files->clear();
    for (int i=0; i<IDE::instance()->recentFiles.size(); i++) {
        ui->menuRecent_Files->addAction(IDE::instance()->recentFiles[i]);
    }
    ui->menuRecent_Files->addSeparator();
    ui->menuRecent_Files->addAction("Clear Menu");
}

void MainWindow::recentFileMenuAction(QAction* a) {
    if (a->text()=="Clear Menu") {
        IDE::instance()->recentFiles.clear();
        updateRecentFiles("");
    } else {
        openFile(a->text());
    }
}

void MainWindow::recentProjectMenuAction(QAction* a) {
    if (a->text()=="Clear Menu") {
        IDE::instance()->recentProjects.clear();
        updateRecentProjects("");
    } else {
        openProject(a->text());
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
            QDataStream out(&file);
            out << (quint32)0xD539EA12;
            out << (quint32)104;
            out.setVersion(QDataStream::Qt_5_0);
            QStringList openFiles;
            QDir projectDir = QFileInfo(filepath).absoluteDir();
            for (int i=0; i<ui->tabWidget->count(); i++) {
                if (ui->tabWidget->widget(i)!=ui->configuration) {
                    CodeEditor* ce = static_cast<CodeEditor*>(ui->tabWidget->widget(i));
                    if (!ce->filepath.isEmpty())
                        openFiles << projectDir.relativeFilePath(ce->filepath);
                }
            }
            out << openFiles;

            out << QString(""); // Used to be additional include path
            out << (qint32)project.currentDataFileIndex();
            out << project.haveExtraArgs();
            out << project.extraArgs();
            out << project.haveExtraMzn2FznArgs();
            out << project.extraMzn2FznArgs();
            out << project.autoClearOutput();
            out << project.mzn2fznVerbose();
            out << project.mzn2fznOptimize();
            out << project.currentSolver();
            out << (qint32)project.n_solutions();
            out << project.printAll();
            out << project.printStats();
            out << project.haveSolverFlags();
            out << project.solverFlags();
            out << (qint32)project.n_threads();
            out << project.haveSeed();
            out << project.seed();
            out << false; // used to be whether time limit is checked
            out << (qint32)project.timeLimit();
            out << project.solverVerbose();
            out << (qint32)ui->tabWidget->currentIndex();
            QStringList projectFilesRelPath;
            QStringList projectFiles = project.files();
            for (QList<QString>::const_iterator it = projectFiles.begin();
                 it != projectFiles.end(); ++it) {
                projectFilesRelPath << projectDir.relativeFilePath(*it);
            }
            out << projectFilesRelPath;
            out << project.defaultBehaviour();
            out << project.mzn2fznPrintStats();
            out << project.n_compress_solutions();
            project.setModified(false, true);

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
    qint32 p_i;
    bool p_b;

    int dataFileIndex;

    in >> p_s; // Used to be additional include path
    in >> dataFileIndex;
    in >> p_b;
    project.haveExtraArgs(p_b, true);
    in >> p_s;
    project.extraArgs(p_s, true);
    in >> p_b;
    project.haveExtraMzn2FznArgs(p_b, true);
    in >> p_s;
    project.extraMzn2FznArgs(p_s, true);
    if (version==104) {
        in >> p_b;
        project.autoClearOutput(p_b, true);
    } else {
        project.autoClearOutput(false, true);
    }
    in >> p_b;
    project.mzn2fznVerbose(p_b, true);
    in >> p_b;
    project.mzn2fznOptimize(p_b, true);
    in >> p_s;
    project.currentSolver(p_s, true);
    in >> p_i;
    project.n_solutions(p_i, true);
    in >> p_b;
    project.printAll(p_b, true);
    in >> p_b;
    project.printStats(p_b, true);
    in >> p_b;
    project.haveSolverFlags(p_b, true);
    in >> p_s;
    project.solverFlags(p_s, true);
    in >> p_i;
    project.n_threads(p_i, true);
    in >> p_b;
    project.haveSeed(p_b, true);
    in >> p_s;
    project.seed(p_s, true);
    in >> p_b; // used to be whether time limit is checked
    in >> p_i;
    project.timeLimit(p_i, true);
    if (version==102 || version==103 || version==104) {
        in >> p_b;
        project.solverVerbose(p_b, true);
        in >> p_i;
        ui->tabWidget->setCurrentIndex(p_i);
    }
    QStringList projectFilesRelPath;
    if (version==103 || version==104) {
        in >> projectFilesRelPath;
    } else {
        projectFilesRelPath = openFiles;
    }
    if ( (version==103 || version==104) && !in.atEnd()) {
        in >> p_b;
        project.defaultBehaviour(p_b, true);
    } else {
        project.defaultBehaviour(project.n_solutions() == 1 && !project.printAll());
    }
    if (version==104 && !in.atEnd()) {
        in >> p_b;
        project.mzn2fznPrintStats(p_b, true);
    }
    if (version==104 && !in.atEnd()) {
        in >> p_i;
        project.n_compress_solutions(p_i, true);
    }
    bool hasNewMoocFile = false;
    QStringList missingFiles;
    for (int i=0; i<projectFilesRelPath.size(); i++) {
        QFileInfo fi(basePath+projectFilesRelPath[i]);
        if (fi.exists()) {
            if (fi.baseName()=="_mooc")
                hasNewMoocFile = true;
        } else {
            missingFiles.append(basePath+projectFilesRelPath[i]);
        }
    }
    if (missingFiles.empty()) {
        for (int i=0; i<projectFilesRelPath.size(); i++) {
            QFileInfo fi(basePath+projectFilesRelPath[i]);
            if (!hasNewMoocFile || fi.baseName()!="_coursera") {
                project.addFile(ui->projectView, projectSort, basePath+projectFilesRelPath[i]);
            }
        }
    } else {
        QMessageBox::warning(this, "MiniZinc IDE", "Could not find files in project:\n"+missingFiles.join("\n"));
    }

    for (int i=0; i<openFiles.size(); i++) {
        QFileInfo fi(basePath+openFiles[i]);
        if (fi.exists()) {
            openFile(basePath+openFiles[i],false);
        }
    }
    setupDznMenu();
    project.currentDataFileIndex(dataFileIndex, true);

    project.setModified(false, true);

    IDE::instance()->projects.insert(projectPath, this);
    tabChange(ui->tabWidget->currentIndex());
    if (ui->projectExplorerDockWidget->isHidden()) {
        on_actionShow_project_explorer_triggered();
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

void MainWindow::on_conf_solver_activated(const QString &arg1)
{
    if (arg1=="Add new solver...") {
        on_actionManage_solvers_triggered(true);
    }
}

void MainWindow::onClipboardChanged()
{
    ui->actionPaste->setEnabled(!QApplication::clipboard()->text().isEmpty());
}

void MainWindow::on_conf_data_file_activated(const QString &arg1)
{
    if (arg1=="Add data file to project...") {
        int nFiles = ui->conf_data_file->count();
        addFileToProject(true);
        if (nFiles < ui->conf_data_file->count()) {
            ui->conf_data_file->setCurrentIndex(ui->conf_data_file->count()-2);
        }
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
        if (ui->tabWidget->widget(i) != ui->configuration) {
            CodeEditor* ce = static_cast<CodeEditor*>(ui->tabWidget->widget(i));
            ce->setDarkMode(darkMode);
        }
    }
    static_cast<CodeEditor*>(IDE::instance()->cheatSheet->centralWidget())->setDarkMode(darkMode);
}
