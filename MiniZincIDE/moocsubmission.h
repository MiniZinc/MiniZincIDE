#ifndef MOOCSUBMISSION_H
#define MOOCSUBMISSION_H

#include <QDialog>
#include <QTextStream>
#include <QJsonObject>
#include "project.h"

class QNetworkReply;
class MainWindow;

namespace Ui {
class MOOCSubmission;
}

class MOOCSubmission : public QDialog
{
    Q_OBJECT

public:
    explicit MOOCSubmission(MainWindow* mw, MOOCAssignment& cp);
    ~MOOCSubmission();

protected:
    enum State { S_NONE, S_WAIT_PWD, S_WAIT_SUBMIT, S_WAIT_SOLVE } _cur_phase;
    int _current_model;

    QTextStream _output_stream;
    QString _output_string;

    QJsonObject _submission;
    QJsonObject _parts;

    MOOCAssignment& project;
    MainWindow* mw;
    QNetworkReply* reply;

    void disableUI(void);
    void enableUI(void);
    void cancelOperation(void);
    void solveNext(void);

public slots:
    void reject();

private slots:

    void submitToMOOC();
    void rcvSubmissionResponse();
    void solverFinished();

    void on_runButton_clicked();
    void rcvLoginCheckResponse();

    void on_storePassword_toggled(bool checked);
private:
    Ui::MOOCSubmission *ui;
};

#endif // MOOCSUBMISSION_H
