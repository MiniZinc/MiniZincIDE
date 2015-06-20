#ifndef COURSERASUBMISSION_H
#define COURSERASUBMISSION_H

#include <QDialog>
#include <QTextStream>
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
    enum State { S_NONE, S_WAIT_CHALLENGE, S_WAIT_SUBMIT, S_WAIT_SOLVE } _cur_phase;
    int _current_model;
    QString _submission;
    QString _source;

    QTextStream _output_stream;

    QString _login;
    QString _sid;
    QString _ch_resp;
    QString _state;

    CourseraProject& project;
    MainWindow* mw;
    QNetworkReply* reply;

    QByteArray challenge_response(QString passwd, QString challenge);

    void disableUI(void);
    void enableUI(void);
    void cancelOperation(void);

public slots:
    void reject();

private slots:
    void on_checkLoginButton_clicked();

    void get_challenge();
    void rcv_challenge();
    void submit_solution();
    void rcv_solution_reply();
    void solver_finished();
    void goto_next();

    void on_runButton_clicked();

    void on_storePassword_toggled(bool checked);

private:
    Ui::CourseraSubmission *ui;
};

#endif // COURSERASUBMISSION_H
