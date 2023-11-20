#ifndef SOLVER_H
#define SOLVER_H

#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QVariant>
#include <QVector>

struct SolverFlag {
    QString name;
    QString description;
    enum { T_INT, T_INT_RANGE, T_BOOL, T_BOOL_ONOFF, T_FLOAT, T_FLOAT_RANGE, T_STRING, T_OPT, T_SOLVER } t;
    double min;
    double max;
    qlonglong min_ll;
    qlonglong max_ll;
    QStringList options;
    QVariant def;
};

Q_DECLARE_METATYPE(SolverFlag)

struct Solver {
    enum SolverInputType {
        I_FZN,
        I_MZN,
        I_NL,
        I_JSON,
        I_UNKNOWN
    };

    QString configFile;
    QString id;
    QString name;
    QString executable;
    QString executable_resolved;
    QString mznlib;
    QString mznlib_resolved;
    QString version;
    int mznLibVersion;
    QString description;
    QString contact;
    QString website;
    SolverInputType inputType;
    bool needsSolns2Out;
    bool isGUIApplication;
    bool needsMznExecutable;
    bool needsStdlibDir;
    bool needsPathsFile;
    QStringList stdFlags;
    QList<SolverFlag> extraFlags;
    QStringList requiredFlags;
    QStringList defaultFlags;
    QStringList tags;
    QJsonObject json;
    bool isDefaultSolver;
    Solver(void) {}

    Solver(const QJsonObject& json);
    static Solver& lookup(const QString& str);
    static Solver& lookup(const QString& id, const QString& version, bool strict = true);

    bool hasAllRequiredFlags(void);

    bool operator==(const Solver&) const;
};

class SolverConfiguration {
public:
    SolverConfiguration(const Solver& solver, bool builtin = false);

    static SolverConfiguration loadJSON(const QString& filename, QStringList& warnings);
    static SolverConfiguration loadJSON(const QJsonDocument& json, QStringList& warnings);
    static SolverConfiguration loadLegacy(const QJsonDocument& json, QStringList& warnings);

    QString solver;
    const Solver& solverDefinition;
    QString paramFile;
    bool isBuiltin;
    int timeLimit;
    bool printIntermediate;
    int numSolutions;
    int numOptimal;
    bool verboseCompilation;
    bool verboseSolving;
    bool compilationStats;
    bool solvingStats;
    bool outputTiming;
    bool outputObjective;
    int optimizationLevel;
    QStringList additionalData;
    int numThreads;
    QVariant randomSeed;
    bool freeSearch;
    QVariantMap extraOptions;
    QVariantMap solverBackendOptions;
    bool modified;

    ///
    /// \brief Gets the name of this solver config
    /// \return The name of the config
    ///
    QString name(void) const;

    ///
    /// \brief Give the JSON representation of this SolverConfiguration
    /// \return This solver config in JSON format
    ///
    QByteArray toJSON(void) const;

    ///
    /// \brief Give the JSON representation of this SolverConfiguration
    /// \return This solver config as a JSON object
    ///
    QJsonObject toJSONObject(void) const;

    ///
    /// \brief Determines if these two solver configs have compatible basic options
    /// \param sc Another solver configuration
    /// \return Whether the basic options match
    ///
    bool syncedOptionsMatch(const SolverConfiguration& sc) const;

    ///
    /// \brief Returns whether the given stdFlag is supported by the solver for this config
    /// \param flag The standard flag
    /// \return True if the flag is supported and false otherwise
    ///
    bool supports(const QString& flag) const;

    bool operator==(const SolverConfiguration& sc) const;
    bool operator!=(const SolverConfiguration& sc) const { return !(*this == sc); }
};


#endif // SOLVER_H
