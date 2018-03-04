#ifndef SOLVERCONFIGURATION_H
#define SOLVERCONFIGURATION_H

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
    QString solverName;
    QString datafile;
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

    static QVector<SolverConfiguration> defaultConfigs(const QStringList& solverNames);

    bool operator==(const SolverConfiguration& sc) const;
};

#endif // SOLVERCONFIGURATION_H
