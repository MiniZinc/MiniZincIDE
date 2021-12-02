#ifndef IDE_H
#define IDE_H

#include <QApplication>
#include <QFileSystemWatcher>
#include <QNetworkAccessManager>
#include <QTextDocument>
#include <QMainWindow>

#include "codeeditor.h"
#include "darkmodenotifier.h"

#ifdef Q_OS_WIN
#define pathSep ";"
#define fileDialogSuffix "/"
#define MZNOS "win"
#else
#define pathSep ":"
#ifdef Q_OS_MAC
#define fileDialogSuffix "/*"
#define MZNOS "mac"
#else
#define fileDialogSuffix "/"
#define MZNOS "linux"
#endif
#endif

class MainWindow;

class IDEStatistics {
public:
    int errorsShown;
    int errorsClicked;
    int modelsRun;
    QStringList solvers;
    QVariantMap toVariantMap(void);
    IDEStatistics(void);
    void init(QVariant v);
    QByteArray toJson(void);
    void resetCounts(void);
};

class IDE : public QApplication {
    Q_OBJECT
public:
    IDE(int& argc, char* argv[]);
    ~IDE(void);
    struct Doc;
    typedef QMap<QString,Doc*> DMap;
    DMap documents;
    typedef QMap<QString,MainWindow*> PMap;
    PMap projects;
    QSet<MainWindow*> mainWindows;

    QStringList recentFiles;
    QStringList recentProjects;

    QSet<QString> modifiedFiles;
    QTimer modifiedTimer;

    IDEStatistics stats;

    MainWindow* lastDefaultProject;
    QMainWindow* cheatSheet;

    QNetworkAccessManager* networkManager;
    QNetworkReply* versionCheckReply;

    DarkModeNotifier* darkModeNotifier;

#ifdef Q_OS_MAC
    QMenuBar* defaultMenuBar;
    QMenu* recentFilesMenu;
    QMenu* recentProjectsMenu;
public slots:
    void recentFileMenuAction(QAction*);
    void recentProjectMenuAction(QAction*);
public:
#endif

    QFileSystemWatcher fsWatch;

    bool hasFile(const QString& path);
    QPair<QTextDocument*,bool> loadFile(const QString& path, QWidget* parent);
    void loadLargeFile(const QString& path, QWidget* parent);
    QTextDocument* addDocument(const QString& path, QTextDocument* doc, CodeEditor* ce);
    void registerEditor(const QString& path, CodeEditor* ce);
    void removeEditor(const QString& path, CodeEditor* ce);
    void renameFile(const QString& oldPath, const QString& newPath);
    QString appDir(void) const;
    static IDE* instance(void);
    QString getLastPath(void);
    void setLastPath(const QString& path);
    void setEditorFont(QFont font);
    void addRecentFile(const QString& file);
    void addRecentProject(const QString& file);
protected:
    bool event(QEvent *);
protected slots:
    void versionCheckFinished(void);
    void newProject(void);
    void openFile(const QString& filename = QString(""));
    void fileModified(const QString&);
    void fileModifiedTimeout(void);
    void handleFocusChange(QWidget*,QWidget*);
    void onDarkModeChanged(bool darkMode);
public slots:
    void checkUpdate(void);
    void help(void);
};

#endif // IDE_H
