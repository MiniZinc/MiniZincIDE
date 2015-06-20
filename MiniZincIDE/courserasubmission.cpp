#include "courserasubmission.h"
#include "ui_courserasubmission.h"
#include "mainwindow.h"

#include <QCheckBox>
#include <QVBoxLayout>
#include <QMessageBox>
#include <QSettings>

#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrlQuery>
#include <QCryptographicHash>

CourseraSubmission::CourseraSubmission(MainWindow* mw0, CourseraProject& cp) :
    QDialog(mw0), _cur_phase(S_NONE), project(cp), mw(mw0),
    ui(new Ui::CourseraSubmission)
{
    ui->setupUi(this);
    QVBoxLayout* modelLayout = new QVBoxLayout;
    ui->modelBox->setLayout(modelLayout);
    QVBoxLayout* problemLayout = new QVBoxLayout;
    ui->problemBox->setLayout(problemLayout);

    for (int i=0; i<project.models.size(); i++) {
        const CourseraItem& item = project.models.at(i);
        QCheckBox* cb = new QCheckBox(item.name);
        cb->setChecked(true);
        modelLayout->addWidget(cb);
    }
    for (int i=0; i<project.problems.size(); i++) {
        const CourseraItem& item = project.problems.at(i);
        QCheckBox* cb = new QCheckBox(item.name);
        cb->setChecked(true);
        problemLayout->addWidget(cb);
    }
    if (project.models.empty())
        modelLayout->addWidget(new QLabel("none"));
    if (project.problems.empty())
        problemLayout->addWidget(new QLabel("none"));
    _output_stream.setString(&_submission);

    QSettings settings;
    settings.beginGroup("coursera");
    ui->storePassword->setChecked(settings.value("storeLogin",false).toBool());
    ui->login->setText(settings.value("login").toString());
    ui->password->setText(settings.value("password").toString());
    settings.endGroup();
}

CourseraSubmission::~CourseraSubmission()
{
    QSettings settings;
    settings.beginGroup("coursera");
    bool storeLogin = ui->storePassword->isChecked();
    settings.setValue("storeLogin", storeLogin);
    if (storeLogin) {
        settings.setValue("login",ui->login->text());
        settings.setValue("password",ui->password->text());
    } else {
        settings.setValue("login","");
        settings.setValue("password","");
    }
    settings.endGroup();
    delete ui;
}

QByteArray CourseraSubmission::challenge_response(QString passwd, QString challenge)
{
    QByteArray hash = QCryptographicHash::hash(QString(challenge+passwd).toLocal8Bit(), QCryptographicHash::Sha1);
    return hash.toHex();
}

void CourseraSubmission::disableUI()
{
    ui->loginGroup->setEnabled(false);
    ui->modelBox->setEnabled(false);
    ui->problemBox->setEnabled(false);
    ui->runButton->setText("Cancel");
}

void CourseraSubmission::enableUI()
{
    ui->loginGroup->setEnabled(true);
    ui->modelBox->setEnabled(true);
    ui->problemBox->setEnabled(true);
    ui->runButton->setText("Run and submit");
}

void CourseraSubmission::cancelOperation()
{
    switch (_cur_phase) {
    case S_NONE:
        return;
    case S_WAIT_CHALLENGE:
        disconnect(reply, SIGNAL(finished()), this, SLOT(rcv_challenge()));
        break;
    case S_WAIT_SOLVE:
        disconnect(mw, SIGNAL(finished()), this, SLOT(solver_finished()));
        mw->on_actionStop_triggered();
        break;
    case S_WAIT_SUBMIT:
        disconnect(reply, SIGNAL(finished()), this, SLOT(rcv_solution_reply()));
        break;
    }
    ui->textBrowser->insertPlainText("Cancelled.\n");
    _cur_phase = S_NONE;
}

void CourseraSubmission::reject()
{
    cancelOperation();
    QDialog::reject();
}

void CourseraSubmission::on_checkLoginButton_clicked()
{
    _current_model = -2;
    get_challenge();
}

void CourseraSubmission::get_challenge()
{
    QString email = ui->login->text();
    if (email.isEmpty()) {
        QMessageBox::warning(this, "MiniZinc IDE",
                             "Enter an email address for Coursera login!");
        _current_model = -2;
        return;
    }
    if (ui->password->text().isEmpty()) {
        QMessageBox::warning(this, "MiniZinc IDE",
                             "Enter a password for Coursera login!");
        _current_model = -2;
        return;
    }
    QUrl url("https://class.coursera.org/" + project.course + "/assignment/challenge");
    QUrlQuery q;
    q.addQueryItem("email_address", email);
    q.addQueryItem("assignment_part_sid", "6vp6Er9J-dev");
    q.addQueryItem("response_encoding","delim");
    url.setQuery(q);

    QNetworkRequest request;
    request.setUrl(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

    _cur_phase = S_WAIT_CHALLENGE;
    reply = IDE::instance()->networkManager->get(request);
    connect(reply, SIGNAL(finished()), this, SLOT(rcv_challenge()));

}

void CourseraSubmission::rcv_challenge()
{
    disconnect(reply, SIGNAL(finished()), this, SLOT(rcv_challenge()));

    reply->deleteLater();
    QString challenge = reply->readAll();
    QStringList fields = challenge.split("|");

    if (fields.size() < 7) {
        QMessageBox::warning(this, "MiniZinc IDE",
                             "Error: cannot connect to Coursera.\n"+challenge);
        _cur_phase = S_NONE;
        enableUI();
        return;
    }

    _login = fields[2];
    QString ch = fields[4];
    _state = fields[6];

    _ch_resp = challenge_response(ui->password->text(),ch);

    if (_current_model < 0) {
        // Check login information
        _submission = "0";
        _source = "";
        _sid = "6vp6Er9J-dev";
        if (_current_model == -1)
            ui->textBrowser->insertPlainText("Checking login\n");
        submit_solution();
    } else {
        int n_problems = project.problems.size();

        CourseraItem& item =
                _current_model < n_problems ?
                    project.problems[_current_model]
                  : project.models[_current_model-n_problems];
        QStringList allfiles = mw->getProject().files();

        bool foundFile = false;
        for (int i=0; i<allfiles.size(); i++) {
            QFileInfo fi(allfiles[i]);
            if (fi.fileName()==item.model) {
                foundFile = true;
                QFile file(fi.absoluteFilePath());
                if (file.open(QFile::ReadOnly | QFile::Text)) {
                    _source = file.readAll();
                } else {
                    ui->textBrowser->insertPlainText("Error: could not open "+item.name+"\n");
                    ui->textBrowser->insertPlainText("Skipping.\n");
                    goto_next();
                    return;
                }
                break;
            }
        }
        if (!foundFile) {
            ui->textBrowser->insertPlainText("Error: could not find "+item.name+"\n");
            ui->textBrowser->insertPlainText("Skipping.\n");
            goto_next();
            return;
        }
        _sid = item.id;

        if (_current_model < n_problems) {
            _submission.clear();
            connect(mw, SIGNAL(finished()), this, SLOT(solver_finished()));
            ui->textBrowser->insertPlainText("Running "+item.name+"\n");
            _cur_phase = S_WAIT_SOLVE;
            mw->runWithOutput(item.model, item.data, item.timeout, _output_stream);
            return;
        } else {
            // Submit model source code
            _submission = _source;
            _source = "";
            ui->textBrowser->insertPlainText("Submitting model "+item.name+"\n");
            submit_solution();
        }

    }
}

void CourseraSubmission::submit_solution()
{
    QUrl url("https://class.coursera.org/" + project.course + "/assignment/submit");
    QUrlQuery q;
    q.addQueryItem("email_address", _login);
    q.addQueryItem("assignment_part_sid", _sid);
    q.addQueryItem("submission",_submission.toUtf8().toBase64());
    q.addQueryItem("submission_aux",_source.toUtf8().toBase64());
    q.addQueryItem("challenge_response", _ch_resp);
    q.addQueryItem("state",_state);

    QNetworkRequest request;
    request.setUrl(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    _cur_phase = S_WAIT_SUBMIT;
    reply = IDE::instance()->networkManager->post(request,q.toString().toLocal8Bit());
    connect(reply, SIGNAL(finished()), this, SLOT(rcv_solution_reply()));
}

void CourseraSubmission::rcv_solution_reply()
{
    disconnect(reply, SIGNAL(finished()), this, SLOT(rcv_solution_reply()));
    reply->deleteLater();

    QString message = reply->readAll();

    if (_current_model < 0) {
        if (message != "password verified") {
            QMessageBox::warning(this, "MiniZinc IDE",
                                 "Login failed! Message: "+message);
            _current_model = -2;
            return;
        }
        if (_current_model == -2) {
            QMessageBox::information(this, "MiniZinc IDE", "Login successful!");
            return;
        }
    }
    ui->textBrowser->insertPlainText("== "+message+"\n");
    goto_next();
}

void CourseraSubmission::solver_finished()
{
    disconnect(mw, SIGNAL(finished()), this, SLOT(solver_finished()));

    QStringList solutions = _submission.split("----------");
    if (solutions.size() < 2) {
        ui->textBrowser->insertPlainText("== no solution found\n");
        goto_next();
    } else {
        _submission = solutions[solutions.size()-2];
        _submission += "----------\nunknown time, MiniZinc IDE submission";
        ui->textBrowser->insertPlainText("Submitting solution\n");
        submit_solution();
    }
}

void CourseraSubmission::goto_next()
{
    int n_models = project.models.size();
    int n_problems = project.problems.size();

    bool done = false;
    do {
        _current_model++;
        if (_current_model < n_problems) {
            QCheckBox* cb = qobject_cast<QCheckBox*>(ui->problemBox->layout()->itemAt(_current_model)->widget());
            done = cb->isChecked();
        } else if (_current_model < n_models+n_problems) {
            int idx = _current_model - n_problems;
            QCheckBox* cb = qobject_cast<QCheckBox*>(ui->modelBox->layout()->itemAt(idx)->widget());
            done = cb->isChecked();
        }
        if (_current_model >= n_models+n_problems) {
            ui->textBrowser->insertPlainText("Done.\n");
            _cur_phase = S_NONE;
            enableUI();
            return;
        }
    } while (!done);
    get_challenge();
}

void CourseraSubmission::on_runButton_clicked()
{
    if (_cur_phase==S_NONE) {
        _current_model = -1;
        disableUI();
        goto_next();
    } else {
        cancelOperation();
    }
}

void CourseraSubmission::on_storePassword_toggled(bool checked)
{
    if (!checked) {
        QSettings settings;
        settings.beginGroup("coursera");
        settings.setValue("storeLogin", false);
        settings.setValue("login","");
        settings.setValue("password","");
        settings.endGroup();
    }
}
