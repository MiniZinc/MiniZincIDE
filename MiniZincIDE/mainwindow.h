#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QFile>
#include <QProcess>
#include <QTimer>
#include <QLabel>
#include <QSet>

#include "codeeditor.h"

namespace Ui {
class MainWindow;
}

struct Solver {
    QString name;
    QString executable;
    QString mznlib;
    QString backend;
    Solver(const QString& n, const QString& e, const QString& m, const QString& b) :
        name(n), executable(e), mznlib(m), backend(b) {}
    Solver(void) {}
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

private slots:
    void on_actionNew_triggered();

    void openFile(const QString &path = QString());

    void on_actionClose_triggered();

    void on_actionOpen_triggered();

    void tabCloseRequest(int);

    void tabChange(int);

    void on_actionRun_triggered();

    void readOutput();

    void procFinished(int);

    void procError(QProcess::ProcessError);

    void on_actionSave_triggered();
    void on_actionQuit_triggered();

    void statusTimerEvent();

    void on_actionStop_triggered();

    void on_actionCompile_triggered();

    void on_actionConstraint_Graph_triggered();

    void openCompiledFzn(int);

protected:
    virtual void closeEvent(QCloseEvent*);
private:
    Ui::MainWindow *ui;
    CodeEditor* curEditor;
    QProcess* process;
    QTimer* timer;
    int time;
    QLabel* statusLabel;
    void createEditor(QFile& file);
    QStringList parseConf(bool compileOnly);
    QSet<QString> filePaths;
    QVector<Solver> solvers;
    QString currentFznTarget;
    void addFile(const QString& path);
    void removeFile(const QString& path);
};

#endif // MAINWINDOW_H
