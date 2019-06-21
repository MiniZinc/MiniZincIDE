#include "solverconfiguration.h"

SolverConfiguration::SolverConfiguration()
{
}

SolverConfiguration SolverConfiguration::defaultConfig() {
    SolverConfiguration def;
    def.isBuiltin = true;
    def.timeLimit = 0;
    def.defaultBehaviour = true;
    def.printIntermediate = false;
    def.stopAfter = 1;
    def.compressSolutionOutput = 100;
    def.clearOutputWindow = false;
    def.verboseFlattening = false;
    def.flatteningStats = false;
    def.optimizationLevel = 1;
    def.additionalData = "";
    def.additionalCompilerCommandline = "";
    def.nThreads = 1;
    def.randomSeed = QVariant();
    def.solverFlags = "";
    def.freeSearch = false;
    def.verboseSolving = false;
    def.outputTiming = false;
    def.solvingStats = false;
    def.runSolutionChecker = true;
    def.useExtraOptions = false;
    return def;
}

void SolverConfiguration::defaultConfigs(const QVector<Solver>& solvers,
                                         QVector<SolverConfiguration>& solverConfigs,
                                         int& defaultSolverIdx)
{
    SolverConfiguration def = defaultConfig();
    int j = 0;
    for (int i=0; i<solverConfigs.size(); i++) {
        if (!solverConfigs[i].isBuiltin) {
            solverConfigs[j++] = solverConfigs[i];
        } else {
            for (const Solver& n : solvers) {
                if (solverConfigs[i].name==n.name+" "+n.version) {
                    solverConfigs[j++] = solverConfigs[i];
                    break;
                }
            }
        }
    }
    solverConfigs.resize(j);

    for (const Solver& n : solvers) {
        if (n.requiredFlags.size()) {
            bool foundAll = true;
            for (auto& rf : n.requiredFlags) {
                if (!n.defaultFlags.contains(rf)) {
                    foundAll=false;
                    break;
                }
            }
            if (!foundAll)
                continue;
        }
        bool haveSolver = false;
        for (int i=0; i<j; i++) {
            if (solverConfigs[i].isBuiltin && solverConfigs[i].name==n.name+" "+n.version) {
                haveSolver = true;
                break;
            }
        }
        if (!haveSolver) {
            def.name = n.name+" "+n.version;
            def.solverId = n.id;
            def.solverVersion = n.version;
            def.extraOptions.clear();
            if (n.isDefaultSolver)
                defaultSolverIdx = solverConfigs.size();
            for (auto ef : n.extraFlags) {
                def.extraOptions[ef.name] = ef.def;
            }
            solverConfigs.push_back(def);
        }
    }
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
            sc.optimizationLevel==optimizationLevel &&
            sc.additionalData==additionalData &&
            sc.additionalCompilerCommandline==additionalCompilerCommandline &&
            sc.nThreads==nThreads &&
            sc.randomSeed.isValid()==randomSeed.isValid() &&
            (sc.randomSeed.isValid() ? sc.randomSeed.toInt()==randomSeed.toInt():true) &&
            sc.solverFlags==solverFlags &&
            sc.freeSearch==freeSearch &&
            sc.verboseSolving==verboseSolving &&
            sc.outputTiming==outputTiming &&
            sc.solvingStats==solvingStats &&
            sc.runSolutionChecker==runSolutionChecker &&
            sc.useExtraOptions==useExtraOptions;
}
