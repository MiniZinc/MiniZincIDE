#include "solverconfiguration.h"

SolverConfiguration::SolverConfiguration()
{
}

QVector<SolverConfiguration> SolverConfiguration::defaultConfigs(const QVector<Solver>& solvers)
{
    QVector<SolverConfiguration> ret;

    SolverConfiguration def;
    def.isBuiltin = true;
    def.isBookmark = true;
    def.timeLimit = 0;
    def.defaultBehaviour = true;
    def.printIntermediate = false;
    def.stopAfter = 1;
    def.compressSolutionOutput = 100;
    def.clearOutputWindow = false;
    def.verboseFlattening = false;
    def.flatteningStats = false;
    def.optimizedFlattening = true;
    def.additionalData = "";
    def.additionalCompilerCommandline = "";
    def.nThreads = 1;
    def.randomSeed = QVariant();
    def.solverFlags = "";
    def.verboseSolving = false;
    def.outputTiming = false;
    def.solvingStats = false;
    def.runSolutionChecker = true;

    for (const Solver& n : solvers) {
        if (n.requiredFlags.size())
            continue;
        def.name = n.name+" "+n.version;
        def.solverId = n.id;
        def.solverVersion = n.version;
        ret.push_back(def);
    }

    return ret;
}

bool SolverConfiguration::operator==(const SolverConfiguration &sc) const
{
    return  sc.solverId==solverId &&
            sc.solverVersion==solverVersion &&
            sc.timeLimit==timeLimit &&
            sc.defaultBehaviour==defaultBehaviour &&
            sc.printIntermediate==printIntermediate &&
            sc.stopAfter==stopAfter &&
            sc.compressSolutionOutput==compressSolutionOutput &&
            sc.clearOutputWindow==clearOutputWindow &&
            sc.verboseFlattening==verboseFlattening &&
            sc.flatteningStats==flatteningStats &&
            sc.optimizedFlattening==optimizedFlattening &&
            sc.additionalData==additionalData &&
            sc.additionalCompilerCommandline==additionalCompilerCommandline &&
            sc.nThreads==nThreads &&
            sc.randomSeed.isValid()==randomSeed.isValid() &&
            (sc.randomSeed.isValid() ? sc.randomSeed.toInt()==randomSeed.toInt():true) &&
            sc.solverFlags==solverFlags &&
            sc.verboseSolving==verboseSolving &&
            sc.outputTiming==outputTiming &&
            sc.solvingStats==solvingStats &&
            sc.runSolutionChecker==runSolutionChecker;
}
