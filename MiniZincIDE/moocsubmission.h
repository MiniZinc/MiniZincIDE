#ifndef MOOCSUBMISSION_H
#define MOOCSUBMISSION_H

#include <QDialog>
#include <QTextStream>
#include <QJsonObject>
#include <QNetworkReply>
#include <QFileInfo>

class QNetworkReply;
class MainWindow;

namespace Ui {
class MOOCSubmission;
}

class MOOCAssignmentItem {
public:
    QString id;
    QString model;
    QString data;
    int timeout;
    QString name;
    bool required;
    MOOCAssignmentItem(QString id0, QString model0, QString data0, QString timeout0, QString name0, bool required0 = false)
        : id(id0), model(model0), data(data0), timeout(timeout0.toInt()), name(name0), required(required0) {}
    MOOCAssignmentItem(QString id0, QString model0, QString name0, bool required0 = false)
        : id(id0), model(model0), timeout(-1), name(name0), required(required0) {}
};

class MOOCAssignment {
public:
    QString name;
    QString assignmentKey;
    QString moocName;
    QString moocPasswordString;
    QString submissionURL;
    QList<MOOCAssignmentItem> problems;
    QList<MOOCAssignmentItem> models;

    MOOCAssignment(void) {}
    MOOCAssignment(const QString& file);

private:
    void loadJSON(const QJsonObject& obj, const QFileInfo& fi);
};

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

    bool checkerEnabled;

public slots:
    void reject();

private slots:

    void submitToMOOC();
    void rcvSubmissionResponse();
    void solverFinished();

    void on_runButton_clicked();
    void rcvLoginCheckResponse();
    void rcvErrorResponse(QNetworkReply::NetworkError);

    void on_storePassword_toggled(bool checked);
private:
    Ui::MOOCSubmission *ui;
};

#endif // MOOCSUBMISSION_H
