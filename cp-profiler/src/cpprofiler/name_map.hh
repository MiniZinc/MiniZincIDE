#pragma once

#include <string>
#include <QString>
#include <vector>
#include <unordered_map>

//// Name Map should get the paths file and model and generate a mapping from UGLY to NICE names

/// A line in a paths file consists of three columns:
/// 1. original name
/// 2. maybe nice name (can be X_INTRODUCED_N_)
/// 3. path

namespace cpprofiler
{

struct Location
{
    int sl; // start line
    int sc; // start column
    int el; // end line
    int ec; // end column
    Location() = default;
    Location(int sl_, int sc_, int el_, int ec_) : sl(sl_), sc(sc_), el(el_), ec(ec_) {}
};

struct SymbolRecord
{

    std::string nice_name;
    std::string path;
    Location location;

    SymbolRecord() = default;
    SymbolRecord(const std::string &nname, const std::string &p, const Location &loc)
        : nice_name(nname), path(p), location(loc) {}
};

using SymbolTable = std::unordered_map<std::string, SymbolRecord>;
using ExpressionTable = std::unordered_map<std::string, std::string>;

using NiceName = std::string;
using Path = std::string;

class NameMap
{

    SymbolTable id_map_;
    ExpressionTable expression_map_;

    const NiceName &getNiceName(const std::string &ident) const;
    void addIdExpressionToMap(const std::vector<std::string> &model, const std::string &ident);

  public:

    NameMap();

    /// Read paths and the model files to construct name mapping;
    /// Returns `true` if successful -- `false` otherwise
    bool initialize(const std::string &path_filename, const std::string &model_filename);

    std::string replaceNames(const std::string &text, bool expand = false) const;


    const Path& getPath(const std::string &ident) const;




};

} // namespace cpprofiler