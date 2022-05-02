#include <QtTest>

#include "testide.h"

#include "ide.h"

#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "moocsubmission.h"
#include "ui_moocsubmission.h"

#include <QTcpServer>
#include <QJsonDocument>
#include <QSignalSpy>

void TestIDE::testMoocProject()
{
    TestMocker mock(QString(MINIZINC_IDE_PATH) + "/data/mooc/TestProject/TestProject.mzp");
    MainWindow* w = mock.mw();
    QVERIFY(w->currentModelFile().endsWith("submission.mzn"));
    QCOMPARE(w->ui->actionSubmit_to_MOOC->text(), "Submit to Test Course");
    w->ui->actionSubmit_to_MOOC->trigger();
    QVERIFY(w->moocSubmission->ui->runButton->isEnabled());
    w->moocSubmission->close();
}


void TestIDE::testMoocTerms()
{
    TestMocker mock(QString(MINIZINC_IDE_PATH) + "/data/mooc/TestTerms/TestTerms.mzp");
    MainWindow* w = mock.mw();
    w->ui->actionSubmit_to_MOOC->trigger();
    QCOMPARE(w->moocSubmission->ui->submissionTerms_textBrowser->toPlainText(), "Submission terms");
    QVERIFY(!w->moocSubmission->ui->runButton->isEnabled());
    w->moocSubmission->ui->submissionTerms_checkBox->setChecked(true);
    QVERIFY(w->moocSubmission->ui->runButton->isEnabled());
    w->moocSubmission->close();
}

void TestIDE::testMoocSubmission()
{
    auto* s = new QTcpServer(this);
    s->listen(QHostAddress::LocalHost);

    TestMocker mock(QString(MINIZINC_IDE_PATH) + "/data/mooc/TestProject/TestProject.mzp");
    MainWindow* w = mock.mw();
    w->ui->actionSubmit_to_MOOC->trigger();
    QUrl url;
    url.setScheme("http");
    url.setHost(s->serverAddress().toString());
    url.setPort(s->serverPort());

    w->moocSubmission->project.submissionURL = url.toString();
    w->moocSubmission->ui->login->setText("user@example.com");
    w->moocSubmission->ui->password->setText("password");
    w->moocSubmission->ui->runButton->click();

    QSignalSpy spy(s, &QTcpServer::newConnection);
    QJsonObject parts;
    for (int i = 0; i < 2; i++) {
        // 1st time is auth check, 2nd time is submission
        QVERIFY(spy.wait(30000));
        QVERIFY(s->hasPendingConnections());
        auto* client = s->nextPendingConnection();
        QSignalSpy spy2(client, &QTcpSocket::readyRead);
        QByteArray request;
        while (spy2.wait(100)) {
            // Hack for reading packet data until there's nothing for 100ms
            request.append(client->readAll());
        }
        qDebug() << QString::fromUtf8(request);
        QTextStream ts(client);
        ts << "HTTP/1.1 200 OK\r\n"
           << "\r\n"
           << "{\"message\": \"Success\"}";
        client->close();
        QSignalSpy spy3(client, &QTcpSocket::disconnected);
        QVERIFY(spy3.wait());
        auto raw = request.mid(request.indexOf("\r\n\r\n"));
        auto body = QJsonDocument::fromJson(raw).object();
        QCOMPARE(body["assignmentKey"].toString(), "0GSb2Dj7kA");
        QCOMPARE(body["secret"], w->moocSubmission->ui->password->text());
        QCOMPARE(body["submitterEmail"], w->moocSubmission->ui->login->text());
        QVERIFY(body["parts"].isObject());
        parts = body["parts"].toObject();
    }
    s->deleteLater();

    QString out1 = "% Solution checker report:\n"
                   "% The value of x in the checker is 1\n"
                   "x = 1;\n"
                   "----------\n"
                   "==========\n";

    QString out2 = "% Solution checker report:\n"
                   "% The value of x in the checker is 2\n"
                   "x = 2;\n"
                   "----------\n"
                   "==========\n";

    QFile f(QString(MINIZINC_IDE_PATH) + "/data/mooc/TestProject/models/submission.mzn");
    QVERIFY(f.open(QIODevice::ReadOnly));
    auto out3 = QString::fromUtf8(f.readAll());
    f.close();

    QVector<QPair<QString, QString>> expected({{"4B1rip2kJ0", out1},
                                               {"amDEPcKA4r", out2},
                                               {"1qJ3CBAZdY", out3}});
    for (auto& it : expected) {
        auto actual = parts[it.first].toObject()["output"].toString();
        QCOMPARE(actual.replace("\r", ""), it.second.replace("\r", ""));
    }

    QVERIFY(QTest::qWaitFor([=] () {
        return w->moocSubmission->_cur_phase == MOOCSubmission::S_NONE;
    }));
    w->moocSubmission->close();
}
