#include <QSettings>
#include <QMessageBox>
#include <QDesktopServices>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>
#include <QUrlQuery>
#include <QNetworkReply>
#include <QFileInfo>
#include <QFileDialog>
#include <QPushButton>

#include "ide.h"
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "checkupdatedialog.h"

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
                if (mw==nullptr) {
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
            if (curw != nullptr && (curw->isEmptyProject() || curw==lastDefaultProject)) {
                curw->openFile(file);
                lastDefaultProject = curw;
            } else {
                QStringList files;
                files << file;
                MainWindow* mw = new MainWindow(files);
                if (curw!=nullptr) {
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

    lastDefaultProject = nullptr;

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
        CodeEditor* ce = new CodeEditor(nullptr,":/cheat_sheet.mzn",false,false,editorFont,darkMode,nullptr,nullptr);
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
    if (old==nullptr && newW!=nullptr && !modifiedFiles.empty()) {
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
                            QMessageBox::warning(nullptr, "MiniZinc IDE",
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
    if (activeWindow()!=nullptr) {
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
    QStringList fileNames;
    if (fileName0.isEmpty()) {
        fileNames = QFileDialog::getOpenFileNames(nullptr, tr("Open File"), getLastPath(), "MiniZinc Files (*.mzn *.dzn *.fzn *.json *.mzp *.mzc *.mpc);;Other (*)");
        if (!fileNames.isEmpty()) {
            setLastPath(QFileInfo(fileNames.last()).absolutePath() + fileDialogSuffix);
        }
    } else {
        fileNames << fileName0;
    }
    if (!fileNames.isEmpty()) {
        MainWindow* mw = new MainWindow(fileNames);
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
            if ( (path.endsWith(".dzn") || path.endsWith(".fzn") || path.endsWith(".json")) && file.size() > 5*1024*1024) {
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
            QTextDocument* nd = nullptr;
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
            int button = QMessageBox::information(nullptr,"Update available",
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
