#include "moocsubmission.h"
#include "ui_moocsubmission.h"
#include "ide.h"
#include "mainwindow.h"
#include "exception.h"

#include <QCheckBox>
#include <QVBoxLayout>
#include <QMessageBox>
#include <QSettings>

#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrlQuery>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonArray>

MOOCAssignment::MOOCAssignment(const QString& fileName)
{
    QFile f(fileName);
    QFileInfo fi(f);
    if (!f.open(QFile::ReadOnly)) {
        throw FileError("Failed to open mooc file");
    }
    auto bytes = f.readAll();
    auto doc = QJsonDocument::fromJson(bytes);
    if (doc.isObject()) {
        loadJSON(doc.object(), fi);
    } else {
        throw MoocError("Could not parse mooc file");
    }
}

void MOOCAssignment::loadJSON(const QJsonObject& obj, const QFileInfo& fi)
{
    if (obj.isEmpty() ||
            !obj["assignmentKey"].isString() || !obj["name"].isString() || !obj["moocName"].isString() ||
            !obj["moocPasswordString"].isString() || !obj["submissionURL"].isString() ||
            !obj["solutionAssignments"].isArray() || !obj["modelAssignments"].isArray()) {
        throw MoocError("Could not parse mooc file");
    }

    assignmentKey = obj["assignmentKey"].toString();
    name = obj["name"].toString();
    moocName = obj["moocName"].toString();
    moocPasswordString = obj["moocPasswordString"].toString();
    submissionURL = obj["submissionURL"].toString();
    QJsonArray sols = obj["solutionAssignments"].toArray();
    for (int i=0; i<sols.size(); i++) {
        QJsonObject solO = sols[i].toObject();
        if (!sols[i].isObject() || !solO["id"].isString() || !solO["model"].isString() ||
                !solO["data"].isString() || (!solO["timeout"].isDouble() && !solO["timeout"].isString() ) || !solO["name"].isString()) {
            throw MoocError("Could not parse mooc file");
        }

        QString timeout = solO["timeout"].isDouble() ? QString::number(solO["timeout"].toInt()) : solO["timeout"].toString();
        QFileInfo model_fi(fi.dir(), solO["model"].toString());
        QFileInfo data_fi(fi.dir(), solO["data"].toString());
        MOOCAssignmentItem item(solO["id"].toString(), model_fi.absoluteFilePath(), data_fi.absoluteFilePath(),
                                timeout, solO["name"].toString());
        problems.append(item);
    }
    QJsonArray modelsArray = obj["modelAssignments"].toArray();
    for (int i=0; i<modelsArray.size(); i++) {
        QJsonObject modelO = modelsArray[i].toObject();
        QFileInfo model_fi(fi.dir(), modelO["model"].toString());
        MOOCAssignmentItem item(modelO["id"].toString(), model_fi.absoluteFilePath(), modelO["name"].toString());
        models.append(item);
    }
}

MOOCSubmission::MOOCSubmission(MainWindow* mw0, MOOCAssignment& cp) :
    QDialog(nullptr), _cur_phase(S_NONE), project(cp), mw(mw0),
    ui(new Ui::MOOCSubmission)
{
    ui->setupUi(this);

    setWindowTitle("Submit to "+cp.moocName);

    ui->loginEmailLabel->setText(cp.moocName+" login email:");
    ui->loginTokenLabel->setText(cp.moocPasswordString+":");
    ui->courseraTokenWarningLabel->setHidden(cp.moocName!="Coursera");

    ui->selectedSolver->setText(mw->currentSolverConfigName());

    QVBoxLayout* modelLayout = new QVBoxLayout;
    ui->modelBox->setLayout(modelLayout);
    QVBoxLayout* problemLayout = new QVBoxLayout;
    ui->problemBox->setLayout(problemLayout);

    for (int i=0; i<project.models.size(); i++) {
        const MOOCAssignmentItem& item = project.models.at(i);
        QCheckBox* cb = new QCheckBox(item.name);
        cb->setChecked(true);
        modelLayout->addWidget(cb);
    }
    for (int i=0; i<project.problems.size(); i++) {
        const MOOCAssignmentItem& item = project.problems.at(i);
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

MOOCSubmission::~MOOCSubmission()
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

void MOOCSubmission::disableUI()
{
    ui->loginGroup->setEnabled(false);
    ui->modelBox->setEnabled(false);
    ui->problemBox->setEnabled(false);
    ui->runButton->setText("Abort");
}

void MOOCSubmission::enableUI()
{
    ui->loginGroup->setEnabled(true);
    ui->modelBox->setEnabled(true);
    ui->problemBox->setEnabled(true);
    ui->runButton->setText("Run and submit");
}

void MOOCSubmission::cancelOperation()
{
    switch (_cur_phase) {
    case S_NONE:
        return;
    case S_WAIT_PWD:
        disconnect(mw, SIGNAL(finished()), this, SLOT(rcvLoginCheckResponse()));
        break;
    case S_WAIT_SOLVE:
        disconnect(mw, SIGNAL(finished()), this, SLOT(solverFinished()));
        mw->stop();
        break;
    case S_WAIT_SUBMIT:
        disconnect(reply, SIGNAL(finished()), this, SLOT(rcvSubmissionResponse()));
        break;
    }
    ui->textBrowser->insertPlainText("Aborted.\n");
    _cur_phase = S_NONE;
    enableUI();
}

void MOOCSubmission::reject()
{
    if (_cur_phase != S_NONE &&
            QMessageBox::warning(this, "MiniZinc IDE",
                                 "Do you want to close this window and abort the "+project.moocName+" submission?",
                                 QMessageBox::Close| QMessageBox::Cancel,
                                 QMessageBox::Cancel) == QMessageBox::Cancel) {
        return;
    }
    cancelOperation();
    QDialog::reject();
}

void MOOCSubmission::solveNext() {
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

        MOOCAssignmentItem& item = project.problems[_current_model];
        connect(mw, SIGNAL(finished()), this, SLOT(solverFinished()));
        ui->textBrowser->insertPlainText("Running "+item.name+" with time out " + QString().setNum(item.timeout)+ "s\n");
        _cur_phase = S_WAIT_SOLVE;
        _output_string = "";
        mw->addOutput("<div style='color:orange;'>Running "+project.moocName+" submission "+item.name+"</div><br>\n");

        auto sc = mw->getCurrentSolverConfig();
        SolverConfiguration solverConfig(*sc);
        solverConfig.timeLimit = item.timeout * 1000; // Override config time limit

        QStringList files = { item.data };

        // When we have a checker, use --output-mode checker
        auto mzc = item.model;
        mzc.replace(mzc.length() - 1, 1, "c");
        if (mw->getProject().contains(mzc)) {
            files << mzc;
            solverConfig.extraOptions["output-mode"] = "checker";
        } else if (mw->getProject().contains(mzc + ".mzn")) {
            files << mzc + ".mzn";
            solverConfig.extraOptions["output-mode"] = "checker";
        }

        mw->run(solverConfig, item.model, files, QStringList(), &_output_stream);
        return;
    } else {
        submitToMOOC();
    }
}

void MOOCSubmission::submitToMOOC()
{
    QUrl url(project.submissionURL);
    QNetworkRequest request;
    request.setUrl(url);
    request.setRawHeader(QByteArray("Cache-Control"),QByteArray("no-cache"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    // add models
    QStringList allfiles = mw->getProject().files();
    for (int i=0; i<project.models.size(); i++) {
        const MOOCAssignmentItem& item = project.models.at(i);
        QCheckBox* cb = qobject_cast<QCheckBox*>(ui->modelBox->layout()->itemAt(i)->widget());
        if (cb->isChecked()) {
            QFile file(item.model);
            if (file.open(QFile::ReadOnly | QFile::Text)) {
                QJsonObject output;
                QTextStream ts(&file);
                output["output"] = ts.readAll();
                _parts[item.id] = output;
            } else {
                ui->textBrowser->insertPlainText("Error: could not open "+item.name+"\n");
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
    connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(rcvErrorResponse(QNetworkReply::NetworkError)));
    connect(reply, SIGNAL(finished()), this, SLOT(rcvSubmissionResponse()));
    ui->textBrowser->insertPlainText("Submitting to "+project.moocName+" for grading...\n");
}

void MOOCSubmission::rcvSubmissionResponse()
{
    disconnect(reply, SIGNAL(finished()), this, SLOT(rcvSubmissionResponse()));
    disconnect(reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(rcvErrorResponse(QNetworkReply::NetworkError)));
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

void MOOCSubmission::solverFinished()
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

void MOOCSubmission::on_runButton_clicked()
{
    if (_cur_phase==S_NONE) {
        ui->textBrowser->clear();
        QString email = ui->login->text();
        if (email.isEmpty()) {
            QMessageBox::warning(this, "MiniZinc IDE",
                                 "Enter an email address for "+project.moocName+" login!");
            return;
        }
        if (ui->password->text().isEmpty()) {
            QMessageBox::warning(this, "MiniZinc IDE",
                                 "Enter an assignment key!");
            return;
        }

        // Send empty request to check password
        QUrl url(project.submissionURL);
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
        connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(rcvErrorResponse(QNetworkReply::NetworkError)));
        connect(reply, SIGNAL(finished()), this, SLOT(rcvLoginCheckResponse()));
        ui->textBrowser->insertPlainText("Checking login and assignment token...\n");
    } else {
        cancelOperation();
    }
}

void MOOCSubmission::rcvLoginCheckResponse()
{
    disconnect(reply, SIGNAL(finished()), this, SLOT(rcvLoginCheckResponse()));
    disconnect(reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(rcvErrorResponse(QNetworkReply::NetworkError)));
    reply->deleteLater();

    QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    if (doc.object().contains("message") && (doc.object()["message"].toString().endsWith("but found: Set()") || doc.object()["message"].toString().endsWith("Success"))) {
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
        ui->textBrowser->insertPlainText("Done.\n");
        _cur_phase = S_NONE;
        enableUI();
    }
}

void MOOCSubmission::rcvErrorResponse(QNetworkReply::NetworkError)
{
    if (!reply->errorString().endsWith("Bad Request")) {
        ui->textBrowser->insertPlainText("Error:\n"+reply->errorString()+"\n\n");
    }
}

void MOOCSubmission::on_storePassword_toggled(bool checked)
{
    if (!checked) {
        QSettings settings;
        settings.beginGroup("coursera");
        settings.remove("");
        settings.setValue("storeLogin", false);
        settings.endGroup();
    }
}
