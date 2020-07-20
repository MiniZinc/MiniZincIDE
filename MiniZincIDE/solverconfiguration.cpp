#include "solverconfiguration.h"
#include "process.h"
#include "exception.h"

#include <QFileInfo>
#include <QFile>
#include <QJsonArray>
#include <QDir>

SolverConfiguration::SolverConfiguration(const Solver& _solver, bool builtin) :
    solverDefinition(_solver),
    isBuiltin(builtin),
    timeLimit(0),
    printIntermediate(true),
    numSolutions(1),
    numOptimal(1),
    verboseCompilation(false),
    verboseSolving(false),
    compilationStats(false),
    solvingStats(false),
    outputTiming(false),
    optimizationLevel(1),
    numThreads(1),
    freeSearch(false),
    useExtraOptions(false),
    modified(false)
{
    solver = _solver.id + "@" + _solver.version;
}

SolverConfiguration SolverConfiguration::loadJSON(const QString& filename)
{
    QFile file(filename);
    QFileInfo fi(file);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        throw FileError("Failed to open file " + filename);
    }
    auto json = QJsonDocument::fromJson(file.readAll());
    file.close();
    auto sc = loadJSON(json);
    sc.paramFile = fi.canonicalFilePath();
    return sc;
}

SolverConfiguration SolverConfiguration::loadJSON(const QJsonDocument& json)
{
    auto configObject = json.object();
    QString solverValue = configObject.value("--solver").isUndefined() ? configObject.value("solver").toString() : configObject.value("--solver").toString();
    auto solver = Solver::lookup(solverValue);
    int i = 0;
    auto& solvers = MznDriver::get().solvers();
    for (auto& s : solvers) {
        if (solver == s) {
            // Solver already loaded
            break;
        }
        i++;
    }
    if (i == solvers.length()) {
        solvers.append(solver);
    }
    bool syncedPrintIntermediate = true;
    int syncedNumSolutions = 1;
    int syncedNumOptimal = 1;
    SolverConfiguration sc(solvers[i]);
    for (auto it = configObject.begin(); it != configObject.end(); it++) {
        QString key = (it.key().startsWith("-") || it.key().startsWith("_")) ? it.key() : "--" + it.key();
        if (key == "--solver") {
            sc.solver = it.value().toString();
        } else if (key == "_syncedPrintIntermediate") {
            syncedPrintIntermediate = it.value().toBool(true);
        } else if (key == "_syncedNumSolutions") {
            syncedNumSolutions= it.value().toInt(1);
        } else if (key == "_syncedNumOptimal") {
            syncedNumOptimal = it.value().toInt(1);
        } else if (key == "-t" || key == "--time-limit") {
            sc.timeLimit = it.value().toInt();
        } else if (key == "-a" || key == "--all-solutions") {
            sc.numSolutions = 0;
        } else if (key == "-i" || key == "--intermediate" || key == "--intermediate-solutions") {
            sc.printIntermediate = it.value().toBool(true);
        } else if (key == "-n" || key == "--num-solutions") {
            sc.numSolutions = it.value().toInt(1);
        } else if (key == "-a-o" || key == "--all-optimal") {
            sc.numOptimal = 0;
        } else if (key == "-n-o" || key == "--num-optimal") {
            sc.numOptimal = it.value().toInt(1);
        } else if (key == "--verbose-compilation" || key == "--verbose" || key == "-v") {
            sc.verboseCompilation = it.value().toBool();
        } else if (key == "--verbose-solving" || key == "--verbose" || key == "-v") {
            sc.verboseSolving = it.value().toBool();
        } else if (key == "--compiler-statistics" || key == "--statistics" || key == "-s") {
            sc.compilationStats = it.value().toBool();
        } else if (key == "--solving-statistics" || key == "--statistics" || key == "-s") {
            sc.solvingStats = it.value().toBool();
        } else if (key == "--output-time") {
            sc.outputTiming = it.value().toBool();
        } else if (key == "-O0" && it.value().toBool()) {
            sc.optimizationLevel = 0;
        } else if (key == "-O1" && it.value().toBool()) {
            sc.optimizationLevel = 1;
        } else if (key == "-O2" && it.value().toBool()) {
            sc.optimizationLevel = 2;
        } else if (key == "-O3" && it.value().toBool()) {
            sc.optimizationLevel = 3;
        } else if (key == "-O4" && it.value().toBool()) {
            sc.optimizationLevel = 4;
        } else if (key == "-O5" && it.value().toBool()) {
            sc.optimizationLevel = 5;
        } else if (key == "-D" || key == "--cmdline-data") {
            if (it.value().isArray()) {
                for (auto v : it.value().toArray()) {
                    sc.additionalData.push_back(v.toString());
                }
            } else {
                sc.additionalData.push_back(it.value().toString());
            }
        } else if (key == "-p" || key == "--parallel") {
            sc.numThreads = it.value().toInt(1);
        } else if (key == "-r" || key == "--random-seed") {
            sc.randomSeed = it.value().toVariant();
        } else if (key == "-f" || key == "--free-search") {
            sc.freeSearch = it.value().toBool();
        } else {
            bool matched = false;
            for (auto flag : sc.solverDefinition.extraFlags) {
                if (key == flag.name || key == "_" + flag.name) {
                    matched = true;
                    sc.extraOptions[flag.name] = it.value().toVariant();
                    if (!key.startsWith("_")) {
                        sc.useExtraOptions = true;
                    }
                    break;
                }
            }
            if (!matched) {
                sc.unknownOptions[key] = it.value().toVariant();
            }
        }
    }
    return sc;
}

QString SolverConfiguration::name() const
{
    if (isBuiltin) {
        return solverDefinition.name + " " + solverDefinition.version;
    }

    QString file = paramFile.isEmpty() ?
                "Unsaved Configuration" :
                QFileInfo(paramFile).completeBaseName();

    return file + "(" + solverDefinition.name + " " + solverDefinition.version + ")";
}

///
/// \brief SolverConfiguration::toJSON
/// \return This solver config in JSON format
///
QByteArray SolverConfiguration::toJSON(void) const
{
    QJsonDocument jsonDoc;
    QJsonObject config;
    config["solver"] = solver;
    if (timeLimit > 0) {
        config["time-limit"] = timeLimit;
    }
    if (printIntermediate && (supports("-a") || supports("-i"))) {
        config["intermediate-solutions"] = printIntermediate;
    }
    if (numSolutions > 1 && supports("-n")) {
        config["num-solutions"] = numSolutions;
    }
    if (numSolutions == 0 && supports("-a")) {
        config["all-solutions"] = true;
    }
    if (numOptimal > 1 && supports("-n-o")) {
        config["num-optimal"] = numOptimal;
    }
    if (numOptimal == 0 && supports("-a-o")) {
        config["all-optimal"] = true;
    }
    if (verboseCompilation) {
        config["verbose-compilation"] = verboseCompilation;
    }
    if (verboseSolving && supports("-v")) {
        config["verbose-solving"] = verboseSolving;
    }
    if (compilationStats) {
        config["compiler-statistics"] = compilationStats;
    }
    if (solvingStats && supports("-s")) {
        config["solving-statistics"] = solvingStats;
    }
    if (outputTiming) {
        config["output-time"] = outputTiming;
    }
    if (optimizationLevel != 1) {
        config["-O" + QString::number(optimizationLevel)] = true;
    }
    QJsonArray arr;
    for (auto d : additionalData) {
        arr.push_back(d);
    }
    if (arr.size() > 0) {
        config["cmdline-data"] = arr;
    }
    if (numThreads > 1 && supports("-p")) {
        config["parallel"] = numThreads;
    }
    if (!randomSeed.isNull() && supports("-r")) {
        config["random-seed"] = QJsonValue::fromVariant(randomSeed);
    }
    if (freeSearch && supports("-f")) {
        config["free-search"] = freeSearch;
    }
    for (auto it = extraOptions.begin(); it != extraOptions.end(); it++) {
        config[it.key()] = QJsonValue::fromVariant(it.value());
    }
    for (auto it = unknownOptions.begin(); it != unknownOptions.end(); it++) {
        config[it.key()] = QJsonValue::fromVariant(it.value());
    }
    jsonDoc.setObject(config);
    return jsonDoc.toJson();
}

bool SolverConfiguration::syncedOptionsMatch(const SolverConfiguration& sc) const
{
    if (isBuiltin || sc.isBuiltin) {
        // Built-in configs always can have their options overidden
        return true;
    }

    if (timeLimit != sc.timeLimit) {
        return false;
    }

    if ((supports("-a") || supports("-i")) &&
            (sc.supports("-a") || sc.supports("-i")) &&
            printIntermediate != sc.printIntermediate) {
        return false;
    }

    if (supports("-n") && sc.supports("-n") &&
            (numSolutions > 0 || sc.numSolutions > 0) &&
            numSolutions != sc.numSolutions) {
        return false;
    }

    if (supports("-a") && sc.supports("-a") &&
            (numSolutions == 0 || sc.numSolutions == 0) &&
            numSolutions != sc.numSolutions) {
        return false;
    }

    if (supports("-n-o") && sc.supports("-n-o") &&
            (numOptimal > 0 || sc.numOptimal > 0) &&
            numOptimal != sc.numOptimal) {
        return false;
    }

    if (supports("-a-o") && sc.supports("-a-o") &&
            (numOptimal == 0 || sc.numOptimal == 0) &&
            numOptimal != sc.numOptimal) {
        return false;
    }

    if (verboseCompilation != sc.verboseCompilation) {
        return false;
    }

    if (supports("-v") && sc.supports("-v") &&
            verboseSolving != sc.verboseSolving) {
        return false;
    }

    if (compilationStats != sc.compilationStats) {
        return false;
    }

    if (supports("-s") && sc.supports("-s") &&
            solvingStats != sc.solvingStats) {
        return false;
    }

    if (outputTiming != sc.outputTiming) {
        return false;
    }

    return true;
}

bool SolverConfiguration::supports(const QString &flag) const
{
    return solverDefinition.stdFlags.contains(flag);
}

bool SolverConfiguration::operator==(const SolverConfiguration& sc) const
{
    return solver == sc.solver &&
            solverDefinition.id == sc.solverDefinition.id &&
            solverDefinition.version == sc.solverDefinition.version &&
            isBuiltin == sc.isBuiltin &&
            timeLimit == sc.timeLimit &&
            printIntermediate == sc.printIntermediate &&
            numSolutions == sc.numSolutions &&
            verboseCompilation == sc.verboseCompilation &&
            verboseSolving == sc.verboseSolving &&
            compilationStats == sc.compilationStats &&
            solvingStats == sc.solvingStats &&
            outputTiming == sc.outputTiming &&
            optimizationLevel == sc.optimizationLevel &&
            additionalData == sc.additionalData &&
            numThreads == sc.numThreads &&
            randomSeed == sc.randomSeed &&
            freeSearch == sc.freeSearch &&
            useExtraOptions == sc.useExtraOptions &&
            extraOptions == sc.extraOptions &&
            unknownOptions == sc.unknownOptions;
}
