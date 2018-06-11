#ifndef SOLVERCONFIGURATION_H
#define SOLVERCONFIGURATION_H

#include "solverdialog.h"

#include <QString>
#include <QVariant>
#include <QVector>

class SolverConfiguration
{
public:
    SolverConfiguration();
    QString name;
    bool isBookmark;
    bool isBuiltin;
    QString solverId;
    QString solverVersion;
    int timeLimit;
    bool defaultBehaviour;
    bool printIntermediate;
    int stopAfter;
    int compressSolutionOutput;
    bool clearOutputWindow;
    bool verboseFlattening;
    bool flatteningStats;
    bool optimizedFlattening;
    QString additionalData;
    QString additionalCompilerCommandline;
    int nThreads;
    QVariant randomSeed;
    QString solverFlags;
    bool verboseSolving;
    bool outputTiming;
    bool solvingStats;
    bool runSolutionChecker;

    static void defaultConfigs(const QVector<Solver>& solvers, QVector<SolverConfiguration>& solverConfigs);

    bool operator==(const SolverConfiguration& sc) const;
};

#endif // SOLVERCONFIGURATION_H
