#include "codechecker.h"
#include "process.h"

CodeChecker::~CodeChecker()
{
    cancel();
}

void CodeChecker::connectSignals()
{
    connect(&p, &MznProcess::started, this, &CodeChecker::onStarted);
    connect(&p, &MznProcess::outputStdOut, this, &CodeChecker::onLine);
    connect(&p, &MznProcess::outputStdError, this, &CodeChecker::onLine);
    connect(&p, &MznProcess::finished, this, &CodeChecker::onFinished);
}

void CodeChecker::start(const QString& modelContents, SolverConfiguration& sc, const QString& wd)
{
    cancel();
    connectSignals();
    inRelevantError = false;
    curError = MiniZincError();
    mznErrors.clear();
    SolverConfiguration checkSc(sc.solverDefinition);
    checkSc.additionalData = sc.additionalData;
    checkSc.extraOptions = sc.extraOptions;
    QStringList args;
    args << "--model-check-only" << "-";
    input = modelContents;
    p.start(checkSc, args, wd);
}

void CodeChecker::cancel()
{
    p.disconnect();
    p.terminate();
}

void CodeChecker::onStarted()
{
    p.writeStdIn(input);
    p.closeStdIn();
}

void CodeChecker::onLine(const QString& l)
{
    QJsonParseError error;
    auto json = QJsonDocument::fromJson(l.toUtf8(), &error);
    if (json.isNull()) {
        return;
    }
    auto msg = json.object();
    if (msg["type"] != "error" && msg["type"] != "warning") {
        return;
    }
    if (!msg["location"].isObject()) {
        return;
    }
    auto loc = msg["location"].toObject();
    MiniZincError e;
    e.isWarning = msg["type"] == "warning";
    e.filename = loc["filename"].toString();
    e.first_line = loc["firstLine"].toInt();
    e.first_col = loc["firstColumn"].toInt();
    e.last_line = loc["lastLine"].toInt();
    e.last_col = loc["lastColumn"].toInt();
    e.msg = msg["message"].toString();
    mznErrors.push_back(e);
}

void CodeChecker::onFinished()
{
    emit finished(mznErrors);
}
