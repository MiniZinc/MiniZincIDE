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
    QRegExp errexp("^(.*):(([0-9]+)(\\.([0-9]+)(-([0-9]+)(\\.([0-9]+))?)?)?):\\s*$");
    if (errexp.indexIn(l) != -1) {
        inRelevantError = false;
        QString errFile = errexp.cap(1).trimmed();
        if (errFile=="stdin") {
            inRelevantError = true;
            curError.filename = errFile;
            curError.first_line = errexp.cap(3).toInt();
            curError.first_col = errexp.cap(5).isEmpty() ? 1 : errexp.cap(5).toInt();
            curError.last_line =
                    (errexp.cap(7).isEmpty() || errexp.cap(9).isEmpty()) ? curError.first_line : errexp.cap(7).toInt();
            curError.last_col =
                    errexp.cap(7).isEmpty() ? 1 : (errexp.cap(9).isEmpty() ? errexp.cap(7).toInt(): errexp.cap(9).toInt());
        }
    } else {
        if (inRelevantError && (l.startsWith("MiniZinc:") || l.startsWith("Error:"))) {
            curError.msg = l.trimmed();
            mznErrors.push_back(curError);
        }
    }
}

void CodeChecker::onFinished()
{
    emit finished(mznErrors);
}
