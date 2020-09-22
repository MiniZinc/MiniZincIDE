#include "name_map.hh"
#include "utils/debug.hh"
#include "utils/string_utils.hh"
#include "utils/path_utils.hh"

#include <QDebug>
#include <QString>
#include <QStringList>
#include <fstream>
#include <regex>
#include <utility>

using std::stoi;
using std::string;
using std::vector;

namespace cpprofiler
{

static const std::regex var_name_regex("[A-Za-z][A-Za-z0-9_]*");
static const std::regex assignment_regex("[A-Za-z][A-Za-z0-9_]*=[0-9]*");

static vector<string> read_file_by_lines(const string &file)
{
    std::ifstream model_file(file, std::ifstream::in);

    /// Come back here next time
    if (!model_file.is_open())
    {
        print("ERROR: cannot open model file: {}", file);
        return {};
    }

    vector<string> model_lines;
    string line;
    while (getline(model_file, line))
    {
        model_lines.push_back(line);
    }
    model_file.close();

    return model_lines;
}

std::ostream &operator<<(std::ostream &os, const Location &l)
{
    return os << l.sl << " " << l.sc << " " << l.el << " " << l.ec;
}

/// extract location and whether it is final from a paths line
static std::pair<Location, bool> getLocation(const string &path)
{

    auto model_name = path.substr(0, path.find(utils::minor_sep));

    auto name_pos = path.rfind(model_name);
    auto end_elm = path.find(utils::major_sep, name_pos);

    auto element = path.substr(name_pos, end_elm - name_pos);

    auto loc_parts = utils::split(element, utils::minor_sep);

    auto loc = Location(stoi(loc_parts[1]), stoi(loc_parts[2]),
                        stoi(loc_parts[3]), stoi(loc_parts[4]));

    bool is_final = false;

    if (end_elm == path.size() - 1)
    {
        is_final = true;
    }
    else
    {
        auto rem = path.substr(end_elm + 1);
        auto loc_parts = utils::split(rem, utils::minor_sep, true);

        if (stoi(loc_parts[1]) == 0 && stoi(loc_parts[2]) == 0 &&
            stoi(loc_parts[3]) == 0 && stoi(loc_parts[4]) == 0)
        {
            is_final = true;
        }
    }

    return std::make_pair(loc, is_final);
}

static const Location empty_location{};
static const string empty_string{""};

static const Location &findLocation(const SymbolTable &st,
                                    const string &ident)
{
    auto it = st.find(ident);
    if (it != st.end())
    {
        return it->second.location;
    }
    return empty_location;
}

static const string findPath(const SymbolTable &st, const string &ident)
{
    auto it = st.find(ident);
    if (it != st.end())
    {
        return it->second.path;
    }
    return empty_string;
}

static string extractExpression(const vector<string> &model,
                                const Location &loc)
{
    const auto &line = model.at(loc.sl - 1);
    return line.substr(loc.sc - 1, loc.ec - loc.sc + 1);
}

static string getPathUntilDecomp(const string &path)
{
    const auto &model_name = path.substr(0, path.find(utils::minor_sep));

    const auto last_model_pos = path.rfind(model_name);
    const auto end_pos = path.find(utils::major_sep, last_model_pos);
    return path.substr(0, end_pos);
}

/// expression (template) element
struct TElem
{
    string str;
    bool is_id;
};

std::ostream &operator<<(std::ostream &os, const TElem &el)
{
    return os << el.str << (el.is_id ? "(id)" : "");
}

using Expression = vector<TElem>;

std::string NameMap::replaceNames(const std::string &text, bool expand) const
{

    const auto var_names_begin =
        std::sregex_iterator(text.begin(), text.end(), var_name_regex);
    const auto var_names_end = std::sregex_iterator();

    size_t pos = 0;

    Expression expr;

    /// this might require some caching

    for (auto it = var_names_begin; it != var_names_end; ++it)
    {
        auto match = *it;
        auto str = text.substr(pos, static_cast<size_t>(match.position()) - pos);

        if (match.position() != 0)
        {
            const auto non_id =
                text.substr(pos, static_cast<size_t>(match.position()) - pos);
            expr.push_back({non_id, false});
        }

        const auto id = match.str();
        expr.push_back({id, true});

        pos = static_cast<size_t>(match.position() + match.length());
    }

    expr.push_back({text.substr(pos), false});

    std::stringstream ss;

    for (const auto &el : expr)
    {
        if (el.is_id)
        {
            ss << getNiceName(el.str);
        }
        else
        {
            ss << el.str;
        }
    }

    return ss.str();
}

// static string replaceAssignments(const string& path, const string&
// epxression) {
//   SymbolTable st;

//   auto assignment_begin = std::sregex_iterator(path.begin(), path.end(),
//   assignment_regex); auto assignment_end = std::sregex_iterator();

//   for (auto it = assignment_begin; it != assignment_end; ++it) {
//     std::smatch match = *it;
//     const auto& assign = match.str();
//     const auto& left_right = utils::split(assign, '=');

//     const auto lhs = left_right[0];
//     const auto rhs = left_right[1];

//     print("left: {}, right: {}", lhs, rhs);

//     st[lhs] = SymbolRecord(rhs, empty_string, empty_location);
//   }

//   replaceNames(st, epxression);

//   return "";

// }

void NameMap::addIdExpressionToMap(const vector<string> &model,
                                   const string &ident)
{

    const auto &loc = findLocation(id_map_, ident);

    if (loc.sl == 0)
        return; // default (empty) location?

    const auto &expression = extractExpression(model, loc);
    const auto &path = findPath(id_map_, ident);

    const auto path_until = getPathUntilDecomp(path);

    // replaceAssignments(path_until, expression);
}

NameMap::NameMap() {}

bool NameMap::initialize(const std::string &path_filename,
                         const std::string &model_filename)
{
    vector<string> model_lines = read_file_by_lines(model_filename);
    vector<string> paths_lines = read_file_by_lines(path_filename);

    if (model_lines.size() == 0 || paths_lines.size() == 0)
        return false;

    try
    {

        for (auto &line : paths_lines)
        {
            const auto parts = utils::split(line, '\t');

            const auto id = parts.at(0);
            const auto nice_name = parts.at(1);
            const auto path = parts.at(2);
            const auto loc = getLocation(parts.at(2));

            id_map_.insert({id, SymbolRecord(nice_name, path, loc.first)});

            const auto is_final = loc.second;

            /// TODO: handle complex expressions

            // // if a nice name is not actually nice
            // if (nice_name.substr(0, 12) == "X_INTRODUCED") {
            //   /// example: X_INTRODUCED_16_ should become ...
            //   addIdExpressionToMap(model_lines, parts.at(0));
            // } else {
            //   // addDecompIdExpressionToMap(s[0], modelText);
            // }
        }

        return true;
    }
    catch (std::exception &e)
    {
        qDebug() << "ERR: invalid name map";
        return false;
    }
}

const NiceName &NameMap::getNiceName(const std::string &ident) const
{
    auto it = id_map_.find(ident);
    if (it != id_map_.end())
    {
        return it->second.nice_name;
    }

    return empty_string;
}

const Path &NameMap::getPath(const std::string &ident) const
{

    auto it = id_map_.find(ident);
    if (it != id_map_.end())
    {
        return it->second.path;
    }

    return empty_string;
}

} // namespace cpprofiler