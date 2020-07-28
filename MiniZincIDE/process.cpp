/*
 *  Main authors:
 *     Jason Nguyen <jason.nguyen@monash.edu>
 *     Guido Tack <guido.tack@monash.edu>
 */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <QJsonArray>
#include "process.h"
#include "ide.h"
#include "mainwindow.h"
#include "exception.h"

#ifndef Q_OS_WIN
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <iostream>
#endif

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
        throw ProcessError("Failed to find or launch MiniZinc executable.");
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
        throw DriverError(message);
    }

    auto minVersion = QVersionNumber(2, 2, 0);
    if (_version < minVersion) {
        clear();
        throw DriverError("Versions of MiniZinc before " + minVersion.toString() + " are not supported.");
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
            for (auto item : allSolvers) {
                QJsonObject sj = item.toObject();
                Solver s(sj);
                _solvers.append(s);
            }
        }
    }
}

Solver* MznDriver::defaultSolver(void)
{
    for (auto& solver : solvers()) {
        if (solver.isDefaultSolver) {
            return &solver;
        }
    }
    return nullptr;
}

MznProcess::MznProcess(QObject* parent) :
    Process(parent)
{
    connect(this, &MznProcess::started, [=] {
        elapsedTimer.start();
    });
    connect(this, QOverload<int, QProcess::ExitStatus>::of(&MznProcess::finished), [=](int, QProcess::ExitStatus) {
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

void MznProcess::run(const SolverConfiguration& sc, const QStringList& args, const QString& cwd)
{
    temp = new QTemporaryFile(QDir::tempPath() + "/mzn_XXXXXX.json");
    if (!temp->open()) {
        emit errorOccurred(QProcess::FailedToStart);
        return;
    }
    QString paramFile = temp->fileName();
    temp->write(sc.toJSON());
    temp->close();
    QStringList newArgs;
    newArgs << "--param-file" << paramFile << args;
    setWorkingDirectory(cwd);
    Process::start(MznDriver::get().minizincExecutable(), newArgs, MznDriver::get().mznDistribPath());
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

SolveProcess::SolveProcess(QObject* parent) :
    MznProcess(parent)
{
    connect(this, &SolveProcess::readyReadStandardOutput, this, &SolveProcess::onStdout);
    connect(this, &SolveProcess::readyReadStandardError, this, &SolveProcess::onStderr);
    connect(this, QOverload<int, QProcess::ExitStatus>::of(&SolveProcess::finished), this, &SolveProcess::onFinished);
}

void SolveProcess::solve(const SolverConfiguration& sc, const QString& modelFile, const QStringList& dataFiles, const QStringList& extraArgs)
{
    state = State::Output;
    outputBuffer.clear();
    htmlBuffer.clear();
    jsonBuffer.clear();

    QFileInfo fi(modelFile);
    QStringList args;
    args << extraArgs << modelFile << dataFiles;
    MznProcess::run(sc, args, fi.canonicalPath());
}

void SolveProcess::onStdout()
{
    setReadChannel(QProcess::ProcessChannel::StandardOutput);

    while (canReadLine()) {
        auto line = QString::fromUtf8(readLine());
        processStdout(line);
    }
}

void SolveProcess::onStderr()
{
    setReadChannel(QProcess::ProcessChannel::StandardError);

    while (canReadLine()) {
        auto line = QString::fromUtf8(readLine());
        processStderr(line);
    }
}

void SolveProcess::processStdout(QString line)
{
    auto trimmed = line.trimmed();
    QRegExp jsonPattern("^(?:%%%(top|bottom))?%%%mzn-json(-init)?:(.*)");

    // Can appear in any state
    if (trimmed.startsWith("%%%mzn-stat")) {
        emit statisticOutput(line);
        return;
    } else if (trimmed.startsWith("%%%mzn-progress")) {
        auto split = trimmed.split(" ");
        bool ok = split.length() == 2;
        if (ok) {
            auto progress = split[1].toFloat(&ok);
            if (ok) {
                emit progressOutput(progress);
                return;
            }
        }
    }

    switch (state) {
    case State::Output: // Normal solution output
        if (trimmed == "----------") {
            outputBuffer << line;
            emit solutionOutput(outputBuffer.join(""));
            outputBuffer.clear();
        } else if (trimmed == "==========" || trimmed == "=====UNKNOWN=====" || trimmed == "=====ERROR=====") {
            outputBuffer << line;
            emit finalStatus(outputBuffer.join(""));
            outputBuffer.clear();
        } else if (jsonPattern.exactMatch(trimmed)) {
            auto captures = jsonPattern.capturedTexts();
            state = captures[2] == "-init" ? State::JSONInit : State::JSON;
            QString area = captures[1].isEmpty() ? "top" : captures[1];
            jsonArea = area == "bottom" ? Qt::BottomDockWidgetArea : Qt::TopDockWidgetArea;
            jsonPath = captures[3];
        } else if (trimmed == "%%%mzn-html-start") {
            state = State::HTML;
        } else {
            outputBuffer << line;
        }
        break;
    case State::HTML: // Seen %%%mzn-html-start
        if (trimmed.startsWith("%%%mzn-html-end")) {
            emit htmlOutput(htmlBuffer.join(""));
            state = State::Output;
            htmlBuffer.clear();
        } else {
            htmlBuffer << line;
        }
        break;
    case State::JSONInit: // Seen %%%mzn-json-init
        if (trimmed == "%%%mzn-json-init-end") {
            emit jsonInit(jsonPath, jsonArea, jsonBuffer.join(" "));
            state = State::Output;
            jsonBuffer.clear();
        } else {
            jsonBuffer << trimmed;
        }
        break;
    case State::JSON: // Seen %%%mzn-json
        if (trimmed == "%%%mzn-json-end") {
            emit jsonOutput(jsonPath, jsonArea, jsonBuffer.join(" "));
            state = State::Output;
            jsonBuffer.clear();
        } else if (trimmed == "%%%mzn-json-time") {
            // Wrap current buffer in array with elapsed time
            jsonBuffer.prepend("[");
            jsonBuffer << "," << QString().number(timeElapsed()) << "]";
        } else {
            jsonBuffer << trimmed;
        }
        break;
    }
}

void SolveProcess::processStderr(QString line)
{
    auto trimmed = line.trimmed();
    emit stdErrorOutput(line);
}

void SolveProcess::onFinished(int, QProcess::ExitStatus)
{
    // Finish processing remaining stdout
    onStdout();
    if (!atEnd()) {
        processStdout(readAll());
    }
    if (!outputBuffer.isEmpty()) {
        emit fragment(outputBuffer.join(""));
        outputBuffer.clear();
    }
    // Finish processing remaining stderr
    onStderr();
    if (!atEnd()) {
        processStdout(readAll());
    }
}
