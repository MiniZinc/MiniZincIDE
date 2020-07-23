#ifndef SOLVERCONFIGURATION_H
#define SOLVERCONFIGURATION_H

#include "solverdialog.h"

#include <QJsonDocument>
#include <QString>
#include <QVariant>
#include <QVector>

class SolverConfiguration {
public:
    SolverConfiguration(const Solver& solver, bool builtin = false);

    static SolverConfiguration loadJSON(const QString& filename);
    static SolverConfiguration loadJSON(const QJsonDocument& json);
    static SolverConfiguration loadLegacy(const QJsonDocument& json);

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
    int optimizationLevel;
    QStringList additionalData;
    int numThreads;
    QVariant randomSeed;
    bool freeSearch;
    bool useExtraOptions;
    QVariantMap extraOptions;
    QVariantMap unknownOptions;
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


#endif // SOLVERCONFIGURATION_H
