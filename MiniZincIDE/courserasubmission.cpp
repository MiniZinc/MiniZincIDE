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
#include <QJsonDocument>

CourseraSubmission::CourseraSubmission(MainWindow* mw0, CourseraProject& cp) :
    QDialog(NULL), _cur_phase(S_NONE), project(cp), mw(mw0),
    ui(new Ui::CourseraSubmission)
{
    ui->setupUi(this);

    ui->selectedSolver->setText(mw->getProject().currentSolver());

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
    _output_stream.setString(&_output_string);

    QSettings settings;
    settings.beginGroup("coursera");
    ui->storePassword->setChecked(settings.value("storeLogin",false).toBool());
    ui->login->setText(settings.value("courseraEmail").toString());
    settings.beginGroup(project.assignmentKey);
    ui->password->setText(settings.value("token").toString());
    settings.endGroup();
    settings.endGroup();

}

CourseraSubmission::~CourseraSubmission()
{
    QSettings settings;
    settings.beginGroup("coursera");
    bool storeLogin = ui->storePassword->isChecked();
    settings.setValue("storeLogin", storeLogin);
    if (storeLogin) {
        settings.setValue("courseraEmail",ui->login->text());
        settings.beginGroup(project.assignmentKey);
        settings.setValue("token",ui->password->text());
        settings.endGroup();
    }
    settings.endGroup();
    delete ui;
}

void CourseraSubmission::disableUI()
{
    ui->loginGroup->setEnabled(false);
    ui->modelBox->setEnabled(false);
    ui->problemBox->setEnabled(false);
    ui->runButton->setText("Abort");
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
    case S_WAIT_PWD:
        disconnect(mw, SIGNAL(finished()), this, SLOT(rcvLoginCheckResponse()));
        break;
    case S_WAIT_SOLVE:
        disconnect(mw, SIGNAL(finished()), this, SLOT(solverFinished()));
        mw->on_actionStop_triggered();
        break;
    case S_WAIT_SUBMIT:
        disconnect(reply, SIGNAL(finished()), this, SLOT(rcvSubmissionResponse()));
        break;
    }
    ui->textBrowser->insertPlainText("Aborted.\n");
    _cur_phase = S_NONE;
    enableUI();
}

void CourseraSubmission::reject()
{
    if (_cur_phase != S_NONE &&
            QMessageBox::warning(this, "MiniZinc IDE",
                                 "Do you want to close this window and abort the Coursera submission?",
                                 QMessageBox::Close| QMessageBox::Cancel,
                                 QMessageBox::Cancel) == QMessageBox::Cancel) {
        return;
    }
    cancelOperation();
    QDialog::reject();
}

void CourseraSubmission::solveNext() {
    int n_problems = project.problems.size();

    bool done = false;
    do {
        _current_model++;
        if (_current_model < n_problems) {
            QCheckBox* cb = qobject_cast<QCheckBox*>(ui->problemBox->layout()->itemAt(_current_model)->widget());
            done = cb->isChecked();
        } else {
            done = true;
        }
    } while (!done);
    if (_current_model < n_problems) {

        CourseraItem& item = project.problems[_current_model];
        connect(mw, SIGNAL(finished()), this, SLOT(solverFinished()));
        ui->textBrowser->insertPlainText("Running "+item.name+"\n");
        _cur_phase = S_WAIT_SOLVE;
        _output_string = "";
        mw->addOutput("<div style='color:orange;'>Running Coursera submission "+item.name+"</div><br>\n");
        if (!mw->runWithOutput(item.model, item.data, item.timeout, _output_stream)) {
            ui->textBrowser->insertPlainText("Error: could not run "+item.name+"\n");
            ui->textBrowser->insertPlainText("Skipping.\n");
            solveNext();
        }
        return;
    } else {
        submitToCoursera();
    }
}

void CourseraSubmission::submitToCoursera()
{
    QUrl url("https://www.coursera.org/api/onDemandProgrammingScriptSubmissions.v1");
    QNetworkRequest request;
    request.setUrl(url);
    request.setRawHeader(QByteArray("Cache-Control"),QByteArray("no-cache"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    // add models
    QStringList allfiles = mw->getProject().files();
    for (int i=0; i<project.models.size(); i++) {
        const CourseraItem& item = project.models.at(i);
        QCheckBox* cb = qobject_cast<QCheckBox*>(ui->modelBox->layout()->itemAt(i)->widget());
        if (cb->isChecked()) {
            bool foundFile = false;
            for (int i=0; i<allfiles.size(); i++) {
                QFileInfo fi(allfiles[i]);
                if (fi.fileName()==item.model) {
                    foundFile = true;
                    QFile file(fi.absoluteFilePath());
                    if (file.open(QFile::ReadOnly | QFile::Text)) {
                        QJsonObject output;
                        QTextStream ts(&file);
                        output["output"] = ts.readAll();
                        _parts[item.id] = output;
                    } else {
                        ui->textBrowser->insertPlainText("Error: could not open "+item.name+"\n");
                        ui->textBrowser->insertPlainText("Skipping.\n");
                        solveNext();
                        return;
                    }
                    break;
                }
            }
            if (!foundFile) {
                ui->textBrowser->insertPlainText("Error: could not find "+item.name+"\n");
                ui->textBrowser->insertPlainText("Skipping.\n");
            }
        }
    }

    _submission["assignmentKey"] = project.assignmentKey;
    _submission["secret"] = ui->password->text();
    _submission["submitterEmail"] = ui->login->text();
    _submission["parts"] = _parts;

    QJsonDocument doc(_submission);

    _cur_phase = S_WAIT_SUBMIT;
    reply = IDE::instance()->networkManager->post(request,doc.toJson());
    connect(reply, SIGNAL(finished()), this, SLOT(rcvSubmissionResponse()));
    ui->textBrowser->insertPlainText("Submitting to Coursera for grading...\n");
}

void CourseraSubmission::rcvSubmissionResponse()
{
    disconnect(reply, SIGNAL(finished()), this, SLOT(rcvSubmissionResponse()));
    reply->deleteLater();

    QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    if (doc.object().contains("message")) {
        ui->textBrowser->insertPlainText("== "+doc.object()["message"].toString()+"\n");
    }
    if (doc.object().contains("details")) {
        QJsonObject details = doc.object()["details"].toObject();
        if (details.contains("learnerMessage")) {
            ui->textBrowser->insertPlainText("== "+details["learnerMessage"].toString()+"\n");
        }
    }

    ui->textBrowser->insertPlainText("Done.\n");
    _cur_phase = S_NONE;
    ui->runButton->setText("Done.");
    ui->runButton->setEnabled(false);
    ui->buttonBox->button(QDialogButtonBox::Close)->setDefault(true);
}

void CourseraSubmission::solverFinished()
{
    disconnect(mw, SIGNAL(finished()), this, SLOT(solverFinished()));

    QStringList solutions = _output_string.split("----------");
    if (solutions.size() >= 2) {
        _output_string = solutions[solutions.size()-2]+"----------"+solutions[solutions.size()-1];
    }
    if (_output_string.size()==0 || _output_string[_output_string.size()-1] != '\n')
        _output_string += "\n";

    QJsonObject output;
    output["output"] = _output_string;
    _parts[project.problems[_current_model].id] = output;
    ui->textBrowser->insertPlainText("Finished\n");
    solveNext();
}

void CourseraSubmission::on_runButton_clicked()
{
    if (_cur_phase==S_NONE) {
        ui->textBrowser->clear();
        QString email = ui->login->text();
        if (email.isEmpty()) {
            QMessageBox::warning(this, "MiniZinc IDE",
                                 "Enter an email address for Coursera login!");
            return;
        }
        if (ui->password->text().isEmpty()) {
            QMessageBox::warning(this, "MiniZinc IDE",
                                 "Enter an assignment key!");
            return;
        }

        // Send empty request to check password
        QUrl url("https://www.coursera.org/api/onDemandProgrammingScriptSubmissions.v1");
        QNetworkRequest request;
        request.setUrl(url);
        request.setRawHeader(QByteArray("Cache-Control"),QByteArray("no-cache"));
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        QJsonObject checkPwdSubmission;
        checkPwdSubmission["assignmentKey"] = project.assignmentKey;
        checkPwdSubmission["secret"] = ui->password->text();
        checkPwdSubmission["submitterEmail"] = ui->login->text();
        QJsonObject emptyParts;
        checkPwdSubmission["parts"] = emptyParts;

        QJsonDocument doc(checkPwdSubmission);

        _cur_phase = S_WAIT_PWD;
        disableUI();
        reply = IDE::instance()->networkManager->post(request,doc.toJson());
        connect(reply, SIGNAL(finished()), this, SLOT(rcvLoginCheckResponse()));
        ui->textBrowser->insertPlainText("Checking login and assignment token...\n");
    } else {
        cancelOperation();
    }
}

void CourseraSubmission::rcvLoginCheckResponse()
{
    disconnect(reply, SIGNAL(finished()), this, SLOT(rcvLoginCheckResponse()));
    reply->deleteLater();

    QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    if (doc.object().contains("message") && doc.object()["message"].toString().endsWith("but found: Set()")) {
        ui->textBrowser->insertPlainText("Done.\n");
        _current_model = -1;
        for (int i=0; i<project.problems.size(); i++) {
            _parts[project.problems[i].id] = QJsonObject();
        }
        for (int i=0; i<project.models.size(); i++) {
            _parts[project.models[i].id] = QJsonObject();
        }
        solveNext();
    } else {
        if (doc.object().contains("message")) {
            ui->textBrowser->insertPlainText("== "+doc.object()["message"].toString()+"\n");
        }
        if (doc.object().contains("details")) {
            QJsonObject details = doc.object()["details"].toObject();
            if (details.contains("learnerMessage")) {
                ui->textBrowser->insertPlainText(">> "+details["learnerMessage"].toString()+"\n");
            }
        }
        _cur_phase = S_NONE;
        enableUI();
    }
}

void CourseraSubmission::on_storePassword_toggled(bool checked)
{
    if (!checked) {
        QSettings settings;
        settings.beginGroup("coursera");
        settings.remove("");
        settings.setValue("storeLogin", false);
        settings.endGroup();
    }
}
