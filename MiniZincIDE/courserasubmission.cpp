#include "courserasubmission.h"
#include "ui_courserasubmission.h"
#include "mainwindow.h"

#include <QCheckBox>
#include <QVBoxLayout>

#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrlQuery>
#include <QCryptographicHash>

CourseraSubmission::CourseraSubmission(CourseraProject& cp, QWidget *parent) :
    QDialog(parent), project(cp),
    ui(new Ui::CourseraSubmission)
{
    ui->setupUi(this);
    QVBoxLayout* modelLayout = new QVBoxLayout;
    ui->modelBox->setLayout(modelLayout);
    QVBoxLayout* problemLayout = new QVBoxLayout;
    ui->problemBox->setLayout(problemLayout);

    bool hadProblems = false;
    bool hadModels = false;
    for (unsigned int i=0; i<cp.submissions.size(); i++) {
        const CourseraItem& item = cp.submissions.at(i);
        QCheckBox* cb = new QCheckBox(item.name);
        cb->setChecked(true);
        if (item.timeout==-1) {
            modelLayout->addWidget(cb);
            hadModels = true;
        } else {
            problemLayout->addWidget(cb);
            hadProblems = true;
        }
    }
    if (!hadModels)
        modelLayout->addWidget(new QLabel("none"));
    if (!hadProblems)
        problemLayout->addWidget(new QLabel("none"));
}

CourseraSubmission::~CourseraSubmission()
{
    delete ui;
}

void CourseraSubmission::on_checkLoginButton_clicked()
{
    QUrl url("https://class.coursera.org/" + project.course + "/assignment/challenge");
    QUrlQuery q;
    q.addQueryItem("email_address", "guido.tack@monash.edu");
    q.addQueryItem("assignment_part_sid", "6vp6Er9J-dev");
    q.addQueryItem("response_encoding","delim");
    url.setQuery(q);

    QNetworkRequest request;
    request.setUrl(url);

    reply = IDE::instance()->networkManager->get(request);
    connect(reply, SIGNAL(finished()), this, SLOT(challengeFinished()));
}

void CourseraSubmission::challengeFinished()
{
    disconnect(reply, SIGNAL(finished()), this, SLOT(challengeFinished()));
    QString challenge = reply->readAll();
    QStringList fields = challenge.split("|");
    QString login = fields[2];
    QString ch = fields[4];
    QString state = fields[6];
    QString ch_aux = fields[8];

    QByteArray hash = QCryptographicHash::hash(QString(ch+"QUCfXSfvyz").toLocal8Bit(), QCryptographicHash::Sha1);
    QByteArray ch_resp = hash.toHex();

    QString submission = "0";
    QString source = "";

    QUrl url("https://class.coursera.org/" + project.course + "/assignment/submit");
    QUrlQuery q;
    q.addQueryItem("email_address", login);
    q.addQueryItem("assignment_part_sid", "6vp6Er9J-dev");
    q.addQueryItem("submission",submission.toLocal8Bit().toBase64());
    q.addQueryItem("submission_aux",source.toLocal8Bit().toBase64());
    q.addQueryItem("challenge_response", ch_resp);
    q.addQueryItem("state",state);


    QNetworkRequest request;
    request.setUrl(url);

    reply = IDE::instance()->networkManager->post(request,q.toString().toLocal8Bit());
    connect(reply, SIGNAL(finished()), this, SLOT(responseFinished()));

}

void CourseraSubmission::responseFinished()
{
    disconnect(reply, SIGNAL(finished()), this, SLOT(responseFinished()));
    ui->textBrowser->insertPlainText(reply->readAll());
}
