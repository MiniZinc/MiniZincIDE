/*
 *  Main authors:
 *     Jason Nguyen <jason.nguyen@monash.edu>
 *     Guido Tack <guido.tack@monash.edu>
 */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <QJsonArray>
#include <QJsonDocument>
#include "process.h"
#include "ide.h"
#include "mainwindow.h"

#ifdef Q_OS_WIN
#define pathSep ";"
#else
#define pathSep ":"
#endif

void Process::start(const QString &program, const QStringList &arguments, const QString &path)
{
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    QString curPath = env.value("PATH");
    QString addPath = IDE::instance()->appDir();
    if (!path.isEmpty())
        addPath = path + pathSep + addPath;
    env.insert("PATH", addPath + pathSep + curPath);
    setProcessEnvironment(env);
#ifdef Q_OS_WIN
    _putenv_s("PATH", (addPath + pathSep + curPath).toStdString().c_str());
    jobObject = CreateJobObject(nullptr, nullptr);
    connect(this, SIGNAL(started()), this, SLOT(attachJob()));
#else
    setenv("PATH", (addPath + pathSep + curPath).toStdString().c_str(), 1);
#endif
    QProcess::start(program,arguments, QIODevice::Unbuffered | QIODevice::ReadWrite);
#ifdef Q_OS_WIN
    _putenv_s("PATH", curPath.toStdString().c_str());
#else
    setenv("PATH", curPath.toStdString().c_str(), 1);
#endif
}

void Process::terminate()
{
#ifdef Q_OS_WIN
    QString pipe;
    QTextStream ts(&pipe);
    ts << "\\\\.\\pipe\\minizinc-" << processId();
    auto pipeName = pipe.toStdString();
    HANDLE hNamedPipe = CreateFileA(pipeName.c_str(), GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hNamedPipe) {
        WriteFile(hNamedPipe, nullptr, 0, nullptr, nullptr);
        CloseHandle(hNamedPipe);
    }
#else
    ::killpg(processId(), SIGINT);
#endif
    if (!waitForFinished(500)) {
        if (state() != QProcess::NotRunning) {
#ifdef Q_OS_WIN
            TerminateJobObject(jobObject, EXIT_FAILURE);
#else
            ::killpg(processId(), SIGKILL);
#endif
            if (!waitForFinished(500)) {
                kill();
                waitForFinished();
            }
        }
    }
}

void Process::setupChildProcess()
{
#ifndef Q_OS_WIN
    if (::setpgid(0,0)) {
        std::cerr << "Error: Failed to create sub-process\n";
    }
#endif
}

#ifdef Q_OS_WIN
void Process::attachJob()
{
    AssignProcessToJobObject(jobObject, pid()->hProcess);
}
#endif

void MznDriver::setLocation(const QString &mznDistribPath)
{
    clear();

    _minizincExecutable = "minizinc";
    _mznDistribPath = mznDistribPath;

    MznProcess p;
    p.run({"--version"});
    if (!p.waitForStarted() || !p.waitForFinished()) {
        clear();
        throw QString("Failed to find or launch MiniZinc executable");
    }

    _versionString = p.readAllStandardOutput() + p.readAllStandardError();
    QRegularExpression version_regexp("version (\\d+)\\.(\\d+)\\.(\\d+)");
    QRegularExpressionMatch path_match = version_regexp.match(_versionString);
    if (path_match.hasMatch()) {
        _version = QVersionNumber(
                    path_match.captured(1).toInt(),
                    path_match.captured(2).toInt(),
                    path_match.captured(3).toInt()
        );
    } else {
        QString message = _versionString;
        clear();
        throw message;
    }

    if (_version < QVersionNumber(2, 2, 0)) {
        clear();
        throw QString("Versions of MiniZinc < 2.2.0 are not supported");
    }

    p.run({"--config-dirs"});
    if (p.waitForStarted() && p.waitForFinished()) {
        QString allOutput = p.readAllStandardOutput();
        QJsonDocument jd = QJsonDocument::fromJson(allOutput.toUtf8());
        if (!jd.isNull()) {
            QJsonObject sj = jd.object();
            _userSolverConfigDir = sj["userSolverConfigDir"].toString();
            _userConfigFile = sj["userConfigFile"].toString();
            _mznStdlibDir = sj["mznStdlibDir"].toString();
        }
    }
    p.run({"--solvers-json"});
    if (p.waitForStarted() && p.waitForFinished()) {
        QString allOutput = p.readAllStandardOutput();
        QJsonDocument jd = QJsonDocument::fromJson(allOutput.toUtf8());
        if (!jd.isNull()) {
            _solvers.clear();
            QJsonArray allSolvers = jd.array();
            for (int i=0; i<allSolvers.size(); i++) {
                QJsonObject sj = allSolvers[i].toObject();
                Solver s(sj);
                _solvers.append(s);
            }
        }
    }
}

bool MznDriver::isValid()
{
    return !minizincExecutable().isEmpty();
}

MznProcess::MznProcess(QObject* parent) :
    Process(parent)
{
    connect(this, &MznProcess::started, [=] {
        elapsedTimer.start();
    });
    connect(this, qOverload<int, QProcess::ExitStatus>(&MznProcess::finished), [=](int, QProcess::ExitStatus) {
        onExited();
    });
    connect(this, &MznProcess::errorOccurred, [=](QProcess::ProcessError) {
        onExited();
    });
}

void MznProcess::run(const QStringList& args, const QString& cwd)
{
    assert(state() == NotRunning);
    setWorkingDirectory(cwd);
    Process::start(MznDriver::get().minizincExecutable(), args, MznDriver::get().mznDistribPath());
}

qint64 MznProcess::timeElapsed()
{
    if (elapsedTimer.isValid()) {
        return elapsedTimer.elapsed();
    }
    return finalTime;
}

void MznProcess::onExited()
{
    finalTime = elapsedTimer.elapsed();
    elapsedTimer.invalidate();
    delete temp;
    temp = nullptr;
}
