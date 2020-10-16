#include "solverconfiguration.h"
#include "process.h"
#include "exception.h"

#include <QFileInfo>
#include <QFile>
#include <QJsonParseError>
#include <QJsonArray>
#include <QDir>

namespace {

void parseArgList(const QString& s, QVariantMap& map) {
    QStringList ret;
    bool hadEscape = false;
    bool inSingleQuote = false;
    bool inDoubleQuote = false;
    QString currentArg;
    foreach (const QChar c, s) {
        if (hadEscape) {
            currentArg += c;
            hadEscape = false;
        } else {
            if (c=='\\') {
                hadEscape = true;
            } else if (c=='"') {
                if (inDoubleQuote) {
                    inDoubleQuote=false;
                    ret.push_back(currentArg);
                    currentArg = "";
                } else if (inSingleQuote) {
                    currentArg += c;
                } else {
                    inDoubleQuote = true;
                }
            } else if (c=='\'') {
                if (inSingleQuote) {
                    inSingleQuote=false;
                    ret.push_back(currentArg);
                    currentArg = "";
                } else if (inDoubleQuote) {
                    currentArg += c;
                } else {
                    inSingleQuote = true;
                }
            } else if (!inSingleQuote && !inDoubleQuote && c==' ') {
                if (currentArg.size() > 0) {
                    ret.push_back(currentArg);
                    currentArg = "";
                }
            } else {
                currentArg += c;
            }
        }
    }
    if (currentArg.size() > 0) {
        ret.push_back(currentArg);
    }

    QString flag;
    for (auto& arg : ret) {
        if (arg.startsWith("-")) {
            if (!flag.isEmpty()) {
                // Must be a boolean switch
                map[flag] = true;
            }
            flag = arg;
        } else if (!flag.isEmpty()) {
            // Flag with arg
            map[flag] = arg;
            flag.clear();
        } else {
            // Arg with no flag
            map[arg] = true;
        }
    }
}

}

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
    QJsonParseError error;
    auto json = QJsonDocument::fromJson(file.readAll(), &error);
    file.close();
    if (json.isNull()) {
        QString message;
        QTextStream ts(&message);
        ts << "Could not parse "
           << fi.fileName()
           << ". Error at "
           << error.offset
           << ": "
           << error.errorString()
           << ".";
        throw ConfigError(message);
    }
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
            sc.printIntermediate = true;
        } else if (key == "-i" || key == "--intermediate" || key == "--intermediate-solutions") {
            sc.printIntermediate = it.value().toBool(true);
        } else if (key == "--all-satisfaction") {
            sc.numSolutions = 0;
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
        } else if (key == "--solver-statistics" || key == "--statistics" || key == "-s") {
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
        } else if (key == "-O") {
            sc.optimizationLevel = it.value().toInt();
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
        } else if (key == "--backend-flags" ||
                   key == "--fzn-flags" || key == "--flatzinc-flags" ||
                   key == "--mzn-flags" || key == "--minizinc-flags") {
            if (it.value().isString()) {
                parseArgList(it.value().toString(), sc.solverBackendOptions);
            } else if (it.value().isObject()) {
                auto object = it.value().toObject();
                for (auto it = object.begin(); it != object.end(); it++) {
                    sc.solverBackendOptions.insert(it.key(), it.value().toVariant());
                }
            }
        } else {
            sc.extraOptions[key] = it.value().toVariant();
        }
    }

    for (auto f : solver.extraFlags) {
        if (f.t == SolverFlag::T_BOOL_ONOFF && sc.extraOptions.contains(f.name)) {
            // Convert on/off string to bool (TODO: would be nice to handle this in minizinc)
            sc.extraOptions[f.name] = sc.extraOptions[f.name] == f.options[0];
        }
    }
    return sc;
}

SolverConfiguration SolverConfiguration::loadLegacy(const QJsonDocument &json)
{
    auto sco = json.object();
    auto& solver = Solver::lookup(sco["id"].toString(), sco["version"].toString(), false);

    SolverConfiguration newSc(solver);
//    if (sco["name"].isString()) {
//        newSc.name = sco["name"].toString();
//    }
    if (sco["timeLimit"].isDouble()) {
        newSc.timeLimit = sco["timeLimit"].toInt();
    }
//    if (sco["defaultBehavior"].isBool()) {
//        newSc.defaultBehaviour = sco["defaultBehavior"].toBool();
//    }
    if (sco["printIntermediate"].isBool()) {
        newSc.printIntermediate = sco["printIntermediate"].toBool();
    }
    if (sco["stopAfter"].isDouble()) {
        newSc.numSolutions = sco["stopAfter"].toInt();
    }
    if (sco["verboseFlattening"].isBool()) {
        newSc.verboseCompilation = sco["verboseFlattening"].toBool();
    }
    if (sco["flatteningStats"].isBool()) {
        newSc.compilationStats = sco["flatteningStats"].toBool();
    }
    if (sco["optimizationLevel"].isDouble()) {
        newSc.optimizationLevel = sco["optimizationLevel"].toInt();
    }
    if (sco["additionalData"].isString() && !sco["additionalData"].toString().isEmpty()) {
        newSc.additionalData << sco["additionalData"].toString();
    }
    if (sco["additionalCompilerCommandline"].isString() && !sco["additionalCompilerCommandline"].toString().isEmpty()) {
       parseArgList(sco["additionalCompilerCommandline"].toString(), newSc.extraOptions);
    }
    if (sco["nThreads"].isDouble()) {
        newSc.numThreads = sco["nThreads"].toInt();
    }
    if (sco["randomSeed"].isDouble()) {
        newSc.randomSeed = sco["randomSeed"].toDouble();
    }
    if (sco["solverFlags"].isString() && !sco["solverFlags"].toString().isEmpty()) {
        parseArgList(sco["solverFlags"].toString(), newSc.solverBackendOptions);
    }
    if (sco["freeSearch"].isBool()) {
        newSc.freeSearch = sco["freeSearch"].toBool();
    }
    if (sco["verboseSolving"].isBool()) {
        newSc.verboseSolving = sco["verboseSolving"].toBool();
    }
    if (sco["outputTiming"].isBool()) {
        newSc.outputTiming = sco["outputTiming"].toBool();
    }
    if (sco["solvingStats"].isBool()) {
        newSc.solvingStats = sco["solvingStats"].toBool();
    }
    if (sco["extraOptions"].isObject() && sco["useExtraOptions"].toBool()) {
        QJsonObject extraOptions = sco["extraOptions"].toObject();

        for (auto& f : solver.extraFlags) {
            if (extraOptions.contains(f.name) && f.def.toString() != extraOptions[f.name].toString()) {
                newSc.extraOptions[f.name] = extraOptions[f.name].toString();
            }
        }
    }
    return newSc;
}

QString SolverConfiguration::name() const
{
    QStringList parts;

    if (isBuiltin) {
        parts << solverDefinition.name
              << solverDefinition.version;
    } else {
        if (paramFile.isEmpty()) {
            parts << "Unsaved Configuration";
        } else {
            parts << QFileInfo(paramFile).completeBaseName();
        }
        parts << "(" + solverDefinition.name + " " + solverDefinition.version + ")";
    }

    if (modified) {
        parts << "*";
    }

    return parts.join(" ");
}

QByteArray SolverConfiguration::toJSON(void) const
{
    QJsonDocument jsonDoc;
    jsonDoc.setObject(toJSONObject());
    return jsonDoc.toJson();
}

QJsonObject SolverConfiguration::toJSONObject(void) const
{
    QJsonObject config;
    config["solver"] = solver;
    if (timeLimit > 0) {
        config["time-limit"] = timeLimit;
    }
    if ((supports("-a") || supports("-i"))) {
        config["intermediate-solutions"] = printIntermediate;
    }
    if (numSolutions > 1 && supports("-n")) {
        config["num-solutions"] = numSolutions;
    }
    if (numSolutions == 0 && supports("-a")) {
        config["all-satisfaction"] = true;
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
        config["solver-statistics"] = solvingStats;
    }
    if (outputTiming) {
        config["output-time"] = outputTiming;
    }
    if (optimizationLevel != 1) {
        config["-O"] = optimizationLevel;
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
    if (!solverBackendOptions.empty()) {
        config["backend-flags"] = QJsonObject::fromVariantMap(solverBackendOptions);
    }
    for (auto f : solverDefinition.extraFlags) {
        if (f.t == SolverFlag::T_BOOL_ONOFF && extraOptions.contains(f.name)) {
            // Convert to on/off string instead of bool (TODO: would be nice to handle this in minizinc)
            config[f.name] = extraOptions[f.name].toBool() ? f.options[0] : f.options[1];
        }
    }
    return config;
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
            extraOptions == sc.extraOptions;
}
