#ifndef COURSERASUBMISSION_H
#define COURSERASUBMISSION_H

#include <QDialog>
#include <QTextStream>
#include <QJsonObject>
#include "project.h"

class QNetworkReply;
class MainWindow;

namespace Ui {
class CourseraSubmission;
}

class CourseraSubmission : public QDialog
{
    Q_OBJECT

public:
    explicit CourseraSubmission(MainWindow* mw, CourseraProject& cp);
    ~CourseraSubmission();

protected:
    enum State { S_NONE, S_WAIT_SUBMIT, S_WAIT_SOLVE } _cur_phase;
    int _current_model;

    QTextStream _output_stream;
    QString _output_string;

    QJsonObject _submission;
    QJsonObject _parts;

    CourseraProject& project;
    MainWindow* mw;
    QNetworkReply* reply;

    void disableUI(void);
    void enableUI(void);
    void cancelOperation(void);
    void solveNext(void);

public slots:
    void reject();

private slots:

    void submitToCoursera();
    void rcvSubmissionResponse();
    void solverFinished();

    void on_runButton_clicked();

    void on_storePassword_toggled(bool checked);
private:
    Ui::CourseraSubmission *ui;
};

#endif // COURSERASUBMISSION_H
