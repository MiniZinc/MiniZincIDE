#ifndef COURSERASUBMISSION_H
#define COURSERASUBMISSION_H

#include <QDialog>
#include "project.h"

class QNetworkReply;

namespace Ui {
class CourseraSubmission;
}

class CourseraSubmission : public QDialog
{
    Q_OBJECT

public:
    explicit CourseraSubmission(CourseraProject& cp, QWidget *parent = 0);
    ~CourseraSubmission();

protected:

    CourseraProject& project;
    QNetworkReply* reply;

private slots:
    void on_checkLoginButton_clicked();
    void challengeFinished();
    void responseFinished();
private:
    Ui::CourseraSubmission *ui;
};

#endif // COURSERASUBMISSION_H
