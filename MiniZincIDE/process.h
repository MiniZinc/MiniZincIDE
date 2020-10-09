#ifndef PROCESS_H
#define PROCESS_H

#include <QProcess>
#include <QTextStream>
#include <QVersionNumber>
#include <QElapsedTimer>
#include <QTemporaryFile>

#ifdef Q_OS_WIN
#include <Windows.h>
#endif

#include "solverconfiguration.h"
#include "htmlwindow.h"

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
    const QString& minizincExecutable(void) const
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

    QString _minizincExecutable;
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
class MznProcess : public Process {
    Q_OBJECT
public:
    struct VisOutput {
        VisWindowSpec spec;
        QString data;

        VisOutput(const VisWindowSpec& s = VisWindowSpec(), const QString& d = "") : spec(s), data(d) {}
    };

    MznProcess(QObject* parent=nullptr);

    ///
    /// \brief Start minizinc.
    /// \param args Command line arguments
    /// \param cwd Working directory
    ///
    void run(const QStringList& args, const QString& cwd = QString());

    ///
    /// \brief Start minizinc and use the given solver configuration.
    /// \param sc The solver configuration to use
    /// \param args Command line arguments
    /// \param cwd Working directory
    ///
    void run(const SolverConfiguration& sc, const QStringList& args, const QString& cwd = QString());

    ///
    /// \brief Give the amount of time since solving started
    /// \return Amount of time elapsed in nanoseconds
    ///
    qint64 timeElapsed(void);
private:
    using Process::start;
    QElapsedTimer elapsedTimer;
    qint64 finalTime = 0;
    QTemporaryFile* temp = nullptr;
    void onExited(void);
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
    /// \brief Emitted when a new solution is produced with visualization data.
    /// \param isInitHandler True if this is an initialization handler
    /// \param output The JSON visualization output
    ///
    void jsonOutput(bool isInitHandler, const QVector<VisOutput>& output);
    ///
    /// \brief Emitted when a final status string is read.
    /// \param data The data that was read (==========\n or =====UNKNOWN=====\n or =====ERROR=====\n)
    ///
    void finalStatus(const QString& data);
    ///
    /// \brief Emitted when an unknown fragment is output
    /// \param data The data that was read
    ///
    void fragment(const QString& data);
    ///
    /// \brief Emitted when a line is written to stderr.
    /// \param error The data in stderr.
    ///
    void stdErrorOutput(const QString& error);
    ///
    /// \brief Emitted when output processing has completed and minizinc has exited
    /// \param exitCode The exit code
    /// \param exitStatus The exit status
    ///
    void complete(int exitCode, QProcess::ExitStatus exitStatus);

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
    QVector<VisOutput> visBuffer;

    State state;

    using MznProcess::run;

private slots:
    void onStdout(void);
    void onStderr(void);

    void processStdout(QString line);
    void processStderr(QString line);

    void onFinished(int exitCode, QProcess::ExitStatus exitStatus);
};

#endif // PROCESS_H
