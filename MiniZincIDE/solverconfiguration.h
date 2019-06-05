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
    int optimizationLevel;
    QString additionalData;
    QString additionalCompilerCommandline;
    int nThreads;
    QVariant randomSeed;
    QString solverFlags;
    bool freeSearch;
    bool verboseSolving;
    bool outputTiming;
    bool solvingStats;
    bool runSolutionChecker;
    QMap<QString,QString> extraOptions;

    static void defaultConfigs(const QVector<Solver>& solvers, QVector<SolverConfiguration>& solverConfigs, int& defaultSolverIdx);
    static SolverConfiguration defaultConfig(void);

    bool operator==(const SolverConfiguration& sc) const;
};

#endif // SOLVERCONFIGURATION_H
