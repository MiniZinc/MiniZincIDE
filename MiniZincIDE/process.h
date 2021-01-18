#ifndef PROCESS_H
#define PROCESS_H

#include <QProcess>
#include <QTextStream>
#include <QVersionNumber>
#include <QTemporaryFile>

#ifdef Q_OS_WIN
#define NOMINMAX
#include <Windows.h>
#endif

#include "solverconfiguration.h"
#include "elapsedtimer.h"

///
/// \brief The Process class
/// Extends QProcess with the ability to handle child processes.
///
class Process : public QProcess {
#ifdef Q_OS_WIN
    Q_OBJECT
#endif
public:
    Process(QObject* parent=nullptr) : QProcess(parent) {}
    void start(const QString& program, const QStringList& arguments, const QString& path);
    void terminate(void);
    void sendInterrupt();
protected:
    virtual void setupChildProcess();
#ifdef Q_OS_WIN
private:
    HANDLE jobObject;
private slots:
    void attachJob();
#endif
};

///
/// \brief The MznDriver class
/// Central store of minizinc executable information.
/// Singleton class instantiated with MznDriver::get().
///
class MznDriver {
public:
    static MznDriver& get()
    {
        static MznDriver d;
        return d;
    }

    ///
    /// \brief Set the location of the MiniZinc executable to the given directory
    /// \param mznDistribPath The MiniZinc binary directory
    ///
    void setLocation(const QString& mznDistribPath);

    ///
    /// \brief Returns the validity of the current MiniZinc installation
    /// \return Whether or not a valid MiniZinc installation was found
    ///
    bool isValid(void) const
    {
        return !minizincExecutable().isEmpty();
    }

    MznDriver(MznDriver const&) = delete;
    void operator=(MznDriver const&) = delete;

    ///
    /// \brief The name of the minizinc executable
    /// \return The executable name, or an empty string if not found
    ///
    const QStringList& minizincExecutable(void) const
    {
        return _minizincExecutable;
    }
    ///
    /// \brief The directory that contains the minizinc executable
    /// \return The directory as a string
    ///
    const QString& mznDistribPath(void) const
    {
        return _mznDistribPath;
    }
    ///
    /// \brief The output from running with --version
    /// \return The full output string including stderr
    ///
    const QString& minizincVersionString(void) const
    {
        return _versionString;
    }
    ///
    /// \brief The user solver config directory
    /// \return The directory as a string
    ///
    const QString& userSolverConfigDir(void) const
    {
        return _userSolverConfigDir;
    }
    ///
    /// \brief The location of the user's config file
    /// \return The file location as a string
    ///
    const QString& userConfigFile(void) const
    {
        return _userConfigFile;
    }
    ///
    /// \brief The directory which contains stdlib
    /// \return The directory as a string
    ///
    const QString& mznStdlibDir(void) const
    {
        return _mznStdlibDir;
    }
    ///
    /// \brief The built-in solvers as found by running with --solvers-json
    /// \return The built-in solvers
    ///
    QVector<Solver>& solvers(void)
    {
        return _solvers;
    }

    ///
    /// \brief Get a pointer to the default Solver
    /// \return The default solver or nullptr if there is none
    ///
    Solver* defaultSolver(void);

    ///
    /// \brief Sets the default solver
    /// \param s The new default solver
    ///
    void setDefaultSolver(const Solver& s);

    ///
    /// \brief Returns the version number of MiniZinc
    /// \return The version number
    ///
    const QVersionNumber& version(void)
    {
        return _version;
    }

private:
    MznDriver() {}

    QStringList _minizincExecutable;
    QString _mznDistribPath;
    QString _versionString;
    QString _userSolverConfigDir;
    QString _userConfigFile;
    QString _mznStdlibDir;
    QVector<Solver> _solvers;
    QVersionNumber _version;

    void clear()
    {
        _minizincExecutable.clear();
        _mznDistribPath.clear();
        _versionString.clear();
        _userSolverConfigDir.clear();
        _userConfigFile.clear();
        _mznStdlibDir.clear();
        _solvers.clear();
        _version = QVersionNumber();
    }
};

///
/// \brief The MznProcess class
/// Runs MiniZinc using the current MznDriver
///
class MznProcess : public QObject {
    Q_OBJECT
public:
    struct RunResult {
        int exitCode;
        QString stdOut;
        QString stdErr;

        RunResult(int _exitCode, const QString& _stdOut, const QString& _stdErr)
            : exitCode(_exitCode), stdOut(_stdOut), stdErr(_stdErr) {}
    };

    enum FailureType {
        NonZeroExit,
        FailedToStart,
        Crashed,
        UnknownError
    };
    Q_ENUM(FailureType)

    MznProcess(QObject* parent = nullptr)
        : QObject(parent), ignoreExitStatus(false), p(nullptr), timer(nullptr) {}

    ///
    /// \brief Start minizinc.
    /// \param args Command line arguments
    /// \param cwd Working directory
    ///
    void start(const QStringList& args, const QString& cwd = QString());

    ///
    /// \brief Start minizinc and use the given solver configuration.
    /// \param sc The solver configuration to use
    /// \param args Command line arguments
    /// \param cwd Working directory
    ///
    void start(const SolverConfiguration& sc, const QStringList& args, const QString& cwd = QString());

    ///
    /// \brief Stop minizinc (does not block)
    ///
    void stop();

    ///
    /// \brief Force stop minizinc immediately (blocks, does not finishe processing output)
    ///
    void terminate();

    ///
    /// \brief Run minizinc in blocking mode (only for fast commands).
    /// \param args Command line arguments
    /// \param cwd Working directory
    /// \return The stdout and stderr output
    ///
    RunResult run(const QStringList& args, const QString& cwd = QString());

    ///
    /// \brief Run minizinc in blocking mode (only for fast commands).
    /// \param sc The solver configuration to use
    /// \param args Command line arguments
    /// \param cwd Working directory
    /// \return The stdout and stderr output
    ///
    RunResult run(const SolverConfiguration& sc, const QStringList& args, const QString& cwd = QString());

    ///
    /// \brief Get the time since the process started.
    /// \return The elapsed time in nanoseconds
    ///
    qint64 elapsedTime();

    ///
    /// \brief Write string to process stdin
    /// \param data String data to write
    ///
    void writeStdIn(const QString& data);

    ///
    /// \brief Closes process stdin
    ///
    void closeStdIn();

signals:
    ///
    /// \brief Emitted when the process is started.
    ///
    void started();

    ///
    /// \brief Emitted when a line is written to stdout.
    /// \param output The data in stdout.
    ///
    void outputStdOut(const QString& output);
    ///
    /// \brief Emitted when a line is written to stderr.
    /// \param error The data in stderr.
    ///
    void outputStdError(const QString& error);

    ///
    /// \brief Emitted regularly as time elapses
    /// \param time The time elapsed in nanoseconds
    ///
    void timeUpdated(qint64 time);

    ///
    /// \brief Emitted on successful exit (or after stopping).
    ///
    void success();

    ///
    /// \brief Emitted when the process encounters an error
    /// \param exitCode The exit code
    /// \param e The error that occurred
    ///
    void failure(int exitCode, FailureType e);

    ///
    /// \brief Emitted when finished regardless of success/failure.
    /// \param time The runtime in nanoseconds.
    ///
    void finished(qint64 time);
private:
    bool ignoreExitStatus;
    Process p;
    ElapsedTimer timer;

    void readStdOut();
    void readStdErr();

    void flushOutput();
};

///
/// \brief Runs the minizinc executable and processes solution output.
///
class SolveProcess : public MznProcess {
    Q_OBJECT

public:
    SolveProcess(QObject* parent=nullptr);

    ///
    /// \brief Solve an instance using minizinc, processing solutions
    /// \param sc The solver configuration
    /// \param modelFile The model file path
    /// \param dataFiles The data file paths
    /// \param extraArgs Extra command line arguments
    ///
    void solve(const SolverConfiguration& sc, const QString& modelFile, const QStringList& dataFiles = QStringList(), const QStringList& extraArgs = QStringList());

signals:
    ///
    /// \brief Emitted when ---------- is read.
    /// \param solution The solution including ----------\n
    ///
    void solutionOutput(const QString& solution);
    ///
    /// \brief Emitted when %%%mzn-stat is read.
    /// \param statistic The statistic read
    ///
    void statisticOutput(const QString& statistic);
    ///
    /// \brief Emitted when %%%mzn-html-end is read.
    /// \param html The HTML read
    ///
    void htmlOutput(const QString& html);
    ///
    /// \brief Emitted when %%%mzn-progress is read.
    /// \param progress The progress value
    ///
    void progressOutput(float progress);
    ///
    /// \brief Emitted when a final status string is read.
    /// \param data The data that was read (==========\n or =====UNKNOWN=====\n or =====ERROR=====\n)
    ///
    void finalStatus(const QString& data);
    ///
    /// \brief Emitted when an unknown fragment is output
    /// \param data The data that was readprocess
    ///
    void fragment(const QString& data);

private:
    enum State {
        Output,
        HTML,
        JSONInit,
        JSON
    };

    QStringList outputBuffer;
    QStringList htmlBuffer;
    QStringList jsonBuffer;

    State state;

    using MznProcess::run;

private slots:
    void processStdout(QString line);
    void finished();
};

#endif // PROCESS_H
