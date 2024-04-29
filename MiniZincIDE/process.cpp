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
#else
#include <VersionHelpers.h>
#endif

#ifdef Q_OS_MAC
#include <sys/sysctl.h>
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
    _wputenv_s(L"PATH", (addPath + pathSep + curPath).toStdWString().c_str());
    if (IsWindows8OrGreater()) {
        jobObject = CreateJobObject(nullptr, nullptr);
        connect(this, &QProcess::started, this, &Process::attachJob);
    } else {
        // Workaround PCA automatically adding to a job for Windows 7
        setCreateProcessArgumentsModifier([] (QProcess::CreateProcessArguments *args) {
            args->flags |= CREATE_BREAKAWAY_FROM_JOB;
        });
    }
#else
#if QT_VERSION >= 0x060000
    setChildProcessModifier(Process::setpgid);
#endif
    setenv("PATH", (addPath + pathSep + curPath).toStdString().c_str(), 1);
#endif
    QProcess::start(program,arguments, QIODevice::Unbuffered | QIODevice::ReadWrite);
#ifdef Q_OS_WIN
    _wputenv_s(L"PATH", curPath.toStdWString().c_str());
#else
    setenv("PATH", curPath.toStdString().c_str(), 1);
#endif
}

void Process::terminate()
{
    if (state() != QProcess::NotRunning) {
#ifdef Q_OS_WIN
        if (IsWindows8OrGreater()) {
            TerminateJobObject(jobObject, EXIT_FAILURE);
        } else {
            // We can't use job objects in Windows 7, since MiniZinc already uses them
            QProcess::kill();
        }
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
    if (hNamedPipe != INVALID_HANDLE_VALUE) {
        DWORD bytesWritten;
        WriteFile(hNamedPipe, nullptr, 0, &bytesWritten, nullptr);
        CloseHandle(hNamedPipe);
    }
#else
    ::killpg(processId(), SIGINT);
#endif
}

#if QT_VERSION >= 0x060000
void Process::setpgid()
#else
void Process::setupChildProcess()
#endif
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
    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, true, processId());
    if (hProcess != nullptr) {
        AssignProcessToJobObject(jobObject, hProcess);
    }
}
#endif

void MznDriver::setLocation(const QString &mznDistribPath)
{
    clear();

    _mznDistribPath = mznDistribPath;
    MznProcess p;
    QRegularExpression version_regexp("version (\\d+)\\.(\\d+)\\.(\\d+)");
    _minizincExecutable = QStringList({"minizinc"});

#ifdef Q_OS_MAC
    int isTranslated = 0;
    {
        size_t size = sizeof(isTranslated);
        if (sysctlbyname("sysctl.proc_translated", &isTranslated, &size, NULL, 0) == -1) {
            isTranslated = 0;
        }
    }

    if (isTranslated) {
        _minizincExecutable = QStringList({"arch", "-arch", "arm64", "minizinc"});
    }
    for (int i = 0; i <= isTranslated; i++) {
        if (i == 1) {
            _minizincExecutable = QStringList({"minizinc"});
        }
        try {
            auto result = p.run({"--version"});
            _versionString = result.stdOut + result.stdErr;
            QRegularExpressionMatch path_match = version_regexp.match(_versionString);
            if (path_match.hasMatch()) {
                break;
            }
        } catch (ProcessError& e) {
            if (i == isTranslated) {
                clear();
                throw;
            }
        }
    }
#else
    try {
        auto result = p.run({"--version"});
        _versionString = result.stdOut + result.stdErr;
    } catch (ProcessError&) {
        clear();
        throw;
    }
#endif

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

    auto minVersion = QVersionNumber(2, 8, 0);
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
        for (auto* s : _solvers) {
            delete s;
        }
        _solvers.clear();
        QJsonArray allSolvers = jd.array();
        for (auto item : allSolvers) {
            QJsonObject sj = item.toObject();
            _solvers.append(new Solver(sj));
        }
    }
}

Solver* MznDriver::defaultSolver(void)
{
    for (auto* solver : solvers()) {
        if (solver->isDefaultSolver) {
            return solver;
        }
    }
    return nullptr;
}

void MznDriver::setDefaultSolver(const Solver& s)
{
    for (auto* solver : solvers()) {
        solver->isDefaultSolver = *solver == s;
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
    connect(&p, QOverload<int, QProcess::ExitStatus>::of(&Process::finished), this, [=](int code, QProcess::ExitStatus e) {
        flushOutput();
        timer.stop();
        if (code == 0 || cancelled) {
            emit success(cancelled);
        } else {
            emit failure(code, FailureType::NonZeroExit);
        }
        p.disconnect();
        emit(finished(elapsedTime()));
    });
    connect(&p, &QProcess::errorOccurred, this, [=](QProcess::ProcessError e) {
        timer.stop();
        if (!cancelled) {
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
    cancelled = false;
    const QStringList& exec = MznDriver::get().minizincExecutable();
    QStringList execArgs = args;
    for (unsigned int i = exec.size() - 1; i > 0; i--) {
        execArgs.push_front(exec[i]);
    }
    p.start(exec[0], execArgs, MznDriver::get().mznDistribPath());
}

void MznProcess::start(const SolverConfiguration& sc, const QStringList& args, const QString& cwd, bool jsonStream)
{
    auto* temp = new QTemporaryFile(QDir::tempPath() + "/mzn_XXXXXX.mpc", this);
    if (!temp->open()) {
        emit failure(0, FailureType::FailedToStart);
        return;
    }
    QString paramFile = temp->fileName();
    temp->write(sc.toJSON());
    temp->close();
    connect(this, &MznProcess::finished, temp, [=] () {
        delete temp;
    });
    QStringList newArgs;
    parse = jsonStream;
    if (jsonStream) {
        newArgs << "--json-stream";
    }
    if (!sc.paramFile.isEmpty()) {
        QFileInfo fi(sc.paramFile);
        newArgs << "--push-working-directory"
                << fi.canonicalPath();
    }
    newArgs << "--param-file-no-push" << paramFile;
    if (!sc.paramFile.isEmpty()) {
        newArgs << "--pop-working-directory";
    }
    newArgs << args;
    if (sc.timeLimit != 0) {
        auto* hardTimer = new QTimer(this);
        hardTimer->setSingleShot(true);
        connect(hardTimer, &QTimer::timeout, this, [=] () {
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
    const QStringList& exec = MznDriver::get().minizincExecutable();
    QStringList execArgs = args;
    for (unsigned int i = exec.size() - 1; i > 0; i--) {
        execArgs.push_front(exec[i]);
    }
    p.start(exec[0], execArgs, MznDriver::get().mznDistribPath());
    if (!p.waitForStarted()) {
        throw ProcessError(QString("Failed to find or start %1 %2 in '%3'.")
                               .arg(exec[0])
                               .arg(args.join(" "))
                               .arg(MznDriver::get().mznDistribPath())
                           );
    }
    if (!p.waitForFinished()) {
        p.terminate();
    }
    return { p.exitCode(), p.readAllStandardOutput(), p.readAllStandardError() };
}


MznProcess::RunResult MznProcess::run(const SolverConfiguration& sc, const QStringList& args, const QString& cwd)
{
    QTemporaryFile temp(QDir::tempPath() + "/mzn_XXXXXX.mpc");
    if (!temp.open()) {
        throw ProcessError("Failed to create temporary file");
    }
    QString paramFile = temp.fileName();
    temp.write(sc.toJSON());
    temp.close();
    QStringList newArgs;
    newArgs << "--param-file" << paramFile << args;
    return run(newArgs, cwd);
}

void MznProcess::stop()
{
    if (p.state() == QProcess::NotRunning) {
        return;
    }
    cancelled = true;
    p.sendInterrupt();
    auto* killTimer = new QTimer(this);
    killTimer->setSingleShot(true);
    connect(killTimer, &QTimer::timeout, this, [=] () {
        if (p.state() == QProcess::Running) {
            terminate();
        }
    });
    connect(this, &MznProcess::finished, killTimer, [=] () {
        killTimer->stop();
        delete killTimer;
    });
    killTimer->start(1000);
}

void MznProcess::terminate()
{
    if (p.state() == QProcess::NotRunning) {
        return;
    }

    p.disconnect();
    p.terminate();
    timer.stop();
    emit success(cancelled);
    emit finished(timer.elapsed());
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

QString MznProcess::command() const
{
    return p.program() + " " + p.arguments().join(" ");
}

void MznProcess::processOutput()
{
    p.setReadChannel(QProcess::ProcessChannel::StandardOutput);
    while (p.canReadLine()) {
        auto line = QString::fromUtf8(p.readLine());
        onStdOutLine(line);
    }
    p.setReadChannel(QProcess::ProcessChannel::StandardError);
    while (p.canReadLine()) {
        auto line = QString::fromUtf8(p.readLine());
        emit outputStdError(line);
    }
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
        onStdOutLine(line);
    }
}

void MznProcess::readStdErr()
{
    p.setReadChannel(QProcess::ProcessChannel::StandardError);
    if (p.canReadLine()) {
        auto fragment = QString::fromUtf8(p.readAllStandardOutput());
        if (!fragment.isEmpty()) {
            onStdOutLine(fragment);
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
        onStdOutLine(stdOut);
    }

    auto stdErr = QString::fromUtf8(p.readAllStandardError());
    if (!stdErr.isEmpty()) {
        emit outputStdError(stdOut);
    }
}

void MznProcess::onStdOutLine(const QString& line)
{
    emit outputStdOut(line);

    if (!parse) {
        // Not running with --json-stream, so cannot parse output
        emit unknownOutput(line);
        return;
    }

    QJsonParseError error;
    auto json = QJsonDocument::fromJson(line.toUtf8(), &error);
    if (!json.isNull()) {
        auto msg = json.object();
        auto msg_type = msg["type"].toString();
        if (msg_type == "solution") {
            auto sections = msg["output"].toObject().toVariantMap();
            QStringList order;
            if (msg["sections"].isArray()) {
                for (auto it : msg["sections"].toArray()) {
                    order << it.toString();
                }
            } else {
                order = sections.keys();
            }
            qint64 time = msg["time"].isDouble() ? static_cast<qint64>(msg["time"].toDouble()) : -1;
            emit solutionOutput(sections, order, time);
        } else if (msg_type == "checker") {
            auto checkerSol = [&] (QJsonObject& msg) {
                auto sections = msg["output"].toObject().toVariantMap();
                QStringList order;
                if (msg["sections"].isArray()) {
                    for (auto it : msg["sections"].toArray()) {
                        order << it.toString();
                    }
                } else {
                    order = sections.keys();
                }
                qint64 time = msg["time"].isDouble() ? static_cast<qint64>(msg["time"].toDouble()) : -1;
                emit checkerOutput(sections, order, time);
            };

            if (msg["messages"].isArray()) {
                for (auto it : msg["messages"].toArray()) {
                    auto msg = it.toObject();
                    auto msg_type = msg["type"].toString();
                    if (msg_type == "solution") {
                        checkerSol(msg);
                    } else if (msg_type == "trace") {
                        auto section = msg["section"].toString();
                        qint64 time = msg["time"].isDouble() ? static_cast<qint64>(msg["time"].toDouble()) : -1;
                        emit checkerOutput({{section, msg["message"].toString()}}, {section}, time);
                    } else if (msg_type == "comment") {
                        auto comment = msg["comment"].toString();
                        emit commentOutput(comment);
                    } else if (msg_type == "warning") {
                        emit warningOutput(msg);
                    } else if (msg_type == "error") {
                        emit errorOutput(msg);
                    }
                }
            } else {
                checkerSol(msg);
            }
        } else if (msg_type == "status") {
            qint64 time = msg["time"].isDouble() ? static_cast<qint64>(msg["time"].toDouble()) : -1;
            emit finalStatus(msg["status"].toString(), time);
        } else if (msg_type == "statistics") {
            emit statisticsOutput(msg["statistics"].toObject().toVariantMap());
        } else if (msg_type == "comment") {
            auto comment = msg["comment"].toString();
            emit commentOutput(comment);
        } else if (msg_type == "time") {
            qint64 time = msg["time"].isDouble() ? static_cast<qint64>(msg["time"].toDouble()) : -1;
            emit timeOutput(time);
        } else if (msg_type == "error") {
            emit errorOutput(msg);
        } else if (msg_type == "warning") {
            emit warningOutput(msg);
        } else if (msg_type == "progress") {
            emit progressOutput(msg["progress"].toDouble());
        } else if (msg_type == "paths") {
            QVector<PathEntry> paths;
            for (auto it : msg["paths"].toArray()) {
                paths << it.toObject();
            }
            emit pathsOutput(paths);
        } else if (msg_type == "profiling") {
            QVector<TimingEntry> t;
            for (auto it : msg["entries"].toArray()) {
                t << it.toObject();
            }
            emit profilingOutput(t);
        } else if (msg_type == "trace") {
            emit traceOutput(msg["section"].toString(), msg["message"].toVariant());
        } else {
            emit unknownOutput(line);
        }
        return;
    }

    // Fall back to just printing everything
    emit unknownOutput(line);
}
