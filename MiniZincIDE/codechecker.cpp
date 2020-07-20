#include "codechecker.h"
#include "process.h"

CodeChecker::CodeChecker(QObject *parent) : QObject(parent), p(this)
{
    connect(&p, &MznProcess::started, this, &CodeChecker::onStarted);
    connect(&p, qOverload<int, QProcess::ExitStatus>(&MznProcess::finished), this, &CodeChecker::onFinished);
    connect(&p, &MznProcess::errorOccurred, this, &CodeChecker::onErrorOccurred);

    p.setProcessChannelMode(QProcess::MergedChannels);
}

void CodeChecker::start(const QString& modelContents, SolverConfiguration& sc, const QString& wd)
{
    QStringList args;
    args << "--model-check-only" << "-";
    input = modelContents;
    p.run(sc, args, wd);
}

void CodeChecker::cancel()
{
    p.terminate();
}

void CodeChecker::onStarted()
{
    p.write(input.toUtf8());
    p.closeWriteChannel();
}

void CodeChecker::onFinished(int, QProcess::ExitStatus)
{
    bool inRelevantError = false;
    MiniZincError curError;
    QVector<MiniZincError> mznErrors;
    while (p.canReadLine()) {
        QString l = p.readLine();
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
    emit finished(mznErrors);
}

void CodeChecker::onErrorOccurred(QProcess::ProcessError e)
{
    emit finished({});
}
