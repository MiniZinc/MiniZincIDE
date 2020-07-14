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

#ifdef Q_OS_WIN
#define pathSep ";"
#else
#define pathSep ":"
#endif

void MznProcess::start(const QString &program, const QStringList &arguments, const QString &path)
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

void MznProcess::terminate()
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

void MznProcess::setupChildProcess()
{
#ifndef Q_OS_WIN
    if (::setpgid(0,0)) {
        std::cerr << "Error: Failed to create sub-process\n";
    }
#endif
}

#ifdef Q_OS_WIN
void MznProcess::attachJob()
{
    AssignProcessToJobObject(jobObject, pid()->hProcess);
}
#endif
