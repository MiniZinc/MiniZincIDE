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

void Process::sendInterrupt()
{
    if (state() == QProcess::NotRunning) {
        return;
    }
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
    try {
        auto result = p.run({"--version"});
        _versionString = result.stdOut + result.stdErr;
    } catch (ProcessError&) {
        clear();
        throw;
    }

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

    auto minVersion = QVersionNumber(2, 5, 0);
    if (_version < minVersion) {
        clear();
        throw DriverError("Versions of MiniZinc before " + minVersion.toString() + " are not supported.");
    }

    QString allOutput = p.run({"--config-dirs"}).stdOut;
    QJsonDocument jd = QJsonDocument::fromJson(allOutput.toUtf8());
    if (!jd.isNull()) {
        QJsonObject sj = jd.object();
        _userSolverConfigDir = sj["userSolverConfigDir"].toString();
        _userConfigFile = sj["userConfigFile"].toString();
        _mznStdlibDir = sj["mznStdlibDir"].toString();
    }

    allOutput = p.run({"--solvers-json"}).stdOut.toUtf8();
    jd = QJsonDocument::fromJson(allOutput.toUtf8());
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

Solver* MznDriver::defaultSolver(void)
{
    for (auto& solver : solvers()) {
        if (solver.isDefaultSolver) {
            return &solver;
        }
    }
    return nullptr;
}

void MznDriver::setDefaultSolver(const Solver& s)
{
    for (auto& solver : solvers()) {
        solver.isDefaultSolver = &solver == &s;
    }

    QFile uc(userConfigFile());
    QJsonObject jo;
    if (uc.exists()) {
        if (uc.open(QFile::ReadOnly)) {
            QJsonDocument doc = QJsonDocument::fromJson(uc.readAll());
            if (doc.isNull()) {
                throw DriverError("Cannot modify user configuration file " + userConfigFile());
            }
            jo = doc.object();
            uc.close();
        }
    }
    QJsonArray tagdefs = jo.contains("tagDefaults") ? jo["tagDefaults"].toArray() : QJsonArray();
    bool hadDefault = false;
    for (int i=0; i<tagdefs.size(); i++) {
        if (tagdefs[i].isArray() && tagdefs[i].toArray()[0].isString() && tagdefs[i].toArray()[0].toString().isEmpty()) {
            QJsonArray def = tagdefs[i].toArray();
            def[1] = s.id;
            tagdefs[i] = def;
            hadDefault = true;
            break;
        }
    }
    if (!hadDefault) {
        QJsonArray def;
        def.append("");
        def.append(s.id);
        tagdefs.append(def);
    }
    jo["tagDefaults"] = tagdefs;
    QJsonDocument doc;
    doc.setObject(jo);
    QFileInfo uc_info(userConfigFile());
    if (!QDir().mkpath(uc_info.absoluteDir().absolutePath())) {
        throw DriverError("Cannot create user configuration directory " + uc_info.absoluteDir().absolutePath());
    }
    if (uc.open(QFile::ReadWrite | QIODevice::Truncate)) {
        uc.write(doc.toJson());
        uc.close();
    } else {
        throw DriverError("Cannot write user configuration file " + userConfigFile());
    }
}
void MznProcess::start(const QStringList& args, const QString& cwd)
{
    p.setWorkingDirectory(cwd);

    connect(&timer, &ElapsedTimer::timeElapsed, this, &MznProcess::timeUpdated);
    connect(&p, &QProcess::started, [=] () {
        timer.start(200);
        emit started();
    });
    connect(&p, QOverload<int, QProcess::ExitStatus>::of(&Process::finished), [=](int code, QProcess::ExitStatus e) {
        flushOutput();
        if (code == 0 || ignoreExitStatus) {
            emit success();
        } else {
            emit failure(code, FailureType::NonZeroExit);
        }
        p.disconnect();
        emit(finished(elapsedTime()));
    });
    connect(&p, &QProcess::errorOccurred, [=](QProcess::ProcessError e) {
        if (!ignoreExitStatus) {
            flushOutput();
            switch (e) {
            case QProcess::FailedToStart:
                emit failure(0, FailureType::FailedToStart);
                break;
            case QProcess::Crashed:
                emit failure(p.exitCode(), FailureType::Crashed);
                break;
            default:
                emit failure(p.exitCode(), FailureType::UnknownError);
                break;
            }
            p.disconnect();
            emit(finished(elapsedTime()));
        }
    });
    connect(&p, &QProcess::readyReadStandardOutput, this, &MznProcess::readStdOut);
    connect(&p, &QProcess::readyReadStandardError, this, &MznProcess::readStdErr);

    Q_ASSERT(p.state() == QProcess::NotRunning);
    ignoreExitStatus = false;
    p.start(MznDriver::get().minizincExecutable(), args, MznDriver::get().mznDistribPath());
}

void MznProcess::start(const SolverConfiguration& sc, const QStringList& args, const QString& cwd)
{
    auto* temp = new QTemporaryFile(QDir::tempPath() + "/mzn_XXXXXX.mpc");
    if (!temp->open()) {
        emit failure(0, FailureType::FailedToStart);
        return;
    }
    QString paramFile = temp->fileName();
    temp->write(sc.toJSON());
    temp->close();
    QStringList newArgs;
    newArgs << "--param-file" << paramFile << args;
    if (sc.timeLimit != 0) {
        auto* hardTimer = new QTimer(this);
        hardTimer->setSingleShot(true);
        connect(hardTimer, &QTimer::timeout, hardTimer, [=] () {
            stop();
        });
        connect(this, &MznProcess::started, hardTimer, [=] () {
            hardTimer->start(sc.timeLimit + 1000);
        });
        connect(this, &MznProcess::finished, hardTimer, [=] () {
            delete hardTimer;
        });
    }
    start(newArgs, cwd);
}


MznProcess::RunResult MznProcess::run(const QStringList& args, const QString& cwd)
{
    Q_ASSERT(p.state() == QProcess::NotRunning);
    p.setWorkingDirectory(cwd);
    p.start(MznDriver::get().minizincExecutable(), args, MznDriver::get().mznDistribPath());
    if (!p.waitForStarted()) {
        throw ProcessError("Failed to find or start minizinc " + args.join(" ") + ".");
    }
    if (!p.waitForFinished()) {
        p.terminate();
        throw ProcessError("Failed to run minizinc " + args.join(" ") + ".");
    }
    return { p.exitCode(), p.readAllStandardOutput(), p.readAllStandardError() };
}


MznProcess::RunResult MznProcess::run(const SolverConfiguration& sc, const QStringList& args, const QString& cwd)
{
    auto* temp = new QTemporaryFile(QDir::tempPath() + "/mzn_XXXXXX.mpc");
    if (!temp->open()) {
        throw ProcessError("Failed to create temporary file");
    }
    QString paramFile = temp->fileName();
    temp->write(sc.toJSON());
    temp->close();
    QStringList newArgs;
    newArgs << "--param-file" << paramFile << args;
    return run(newArgs, cwd);
}

void MznProcess::stop()
{
    if (p.state() == QProcess::NotRunning) {
        return;
    }
    ignoreExitStatus = true;
    p.sendInterrupt();
    auto* killTimer = new QTimer(this);
    killTimer->setSingleShot(true);
    connect(killTimer, &QTimer::timeout, killTimer, [=] () {
        if (p.state() == QProcess::Running) {
            terminate();
        }
    });
    connect(this, &MznProcess::finished, [=] () {
        killTimer->stop();
        delete killTimer;
    });
    killTimer->start(200);
}

void MznProcess::terminate()
{
    if (p.state() == QProcess::NotRunning) {
        return;
    }

    p.disconnect();
    p.terminate();
    emit success();
    emit finished(timer.elapsed());
    timer.stop();
}

qint64 MznProcess::elapsedTime()
{
    return timer.elapsed();
}

void MznProcess::writeStdIn(const QString &data)
{
    p.write(data.toUtf8());
}

void MznProcess::closeStdIn()
{
    p.closeWriteChannel();
}

void MznProcess::readStdOut()
{
    p.setReadChannel(QProcess::ProcessChannel::StandardOutput);
    if (p.canReadLine()) {
        auto fragment = QString::fromUtf8(p.readAllStandardError());
        if (!fragment.isEmpty()) {
            emit outputStdError(fragment);
        }
    }
    while (p.canReadLine()) {
        auto line = QString::fromUtf8(p.readLine());
        emit outputStdOut(line);
    }
}

void MznProcess::readStdErr()
{
    p.setReadChannel(QProcess::ProcessChannel::StandardError);
    if (p.canReadLine()) {
        auto fragment = QString::fromUtf8(p.readAllStandardOutput());
        if (!fragment.isEmpty()) {
            emit outputStdOut(fragment);
        }
    }
    while (p.canReadLine()) {
        auto line = QString::fromUtf8(p.readLine());
        emit outputStdError(line);
    }
}

void MznProcess::flushOutput()
{
    readStdOut();
    readStdErr();

    auto stdOut = QString::fromUtf8(p.readAllStandardOutput());
    if (!stdOut.isEmpty()) {
        emit outputStdOut(stdOut);
    }

    auto stdErr = QString::fromUtf8(p.readAllStandardError());
    if (!stdErr.isEmpty()) {
        emit outputStdError(stdOut);
    }
}

SolveProcess::SolveProcess(QObject* parent) :
    MznProcess(parent)
{
    connect(this, &MznProcess::outputStdOut, this, &SolveProcess::processStdout);
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
    MznProcess::start(sc, args, fi.canonicalPath());
}

void SolveProcess::processStdout(QString line)
{
    auto trimmed = line.trimmed();
    QRegExp jsonPattern("^(?:%%%(top|bottom))?%%%mzn-json(-init)?:(.*)");

    // Can appear in any state
    if (trimmed.startsWith("%%%mzn-stat")) {
        if (!outputBuffer.isEmpty()) {
            // For now, also process any output gragment so it appears in the correct order
            emit fragment(outputBuffer.join(""));
            outputBuffer.clear();
        }
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
            if (!visBuffer.isEmpty()) {
                emit jsonOutput(false, visBuffer);
                visBuffer.clear();
            }
            outputBuffer.clear();
        } else if (trimmed == "==========" || trimmed == "=====UNKNOWN=====" || trimmed == "=====ERROR=====") {
            outputBuffer << line;
            emit finalStatus(outputBuffer.join(""));
            outputBuffer.clear();
        } else if (jsonPattern.exactMatch(trimmed)) {
            auto captures = jsonPattern.capturedTexts();
            state = captures[2] == "-init" ? State::JSONInit : State::JSON;
            QString area = captures[1].isEmpty() ? "top" : captures[1];
            auto jsonArea = area == "bottom" ? Qt::BottomDockWidgetArea : Qt::TopDockWidgetArea;
            auto jsonPath = captures[3];
            visBuffer << VisOutput(VisWindowSpec(jsonPath, jsonArea));
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
        if (trimmed == "%%%mzn-json-end") {
            visBuffer.last().data = jsonBuffer.join(" ");
            state = State::Output;
            jsonBuffer.clear();
        } else if (trimmed == "%%%mzn-json-init-end") {
            visBuffer.last().data = jsonBuffer.join(" ");
            emit jsonOutput(true, visBuffer);
            state = State::Output;
            jsonBuffer.clear();
            visBuffer.clear();
        } else {
            jsonBuffer << trimmed;
        }
        break;
    case State::JSON: // Seen %%%mzn-json
        if (trimmed == "%%%mzn-json-end") {
            visBuffer.last().data = jsonBuffer.join(" ");
            state = State::Output;
            jsonBuffer.clear();
        } else if (trimmed == "%%%mzn-json-time") {
            // Wrap current buffer in array with elapsed time
            jsonBuffer.prepend("[");
            jsonBuffer << "," << QString().number(elapsedTime()) << "]";
        } else {
            jsonBuffer << trimmed;
        }
        break;
    }
}
