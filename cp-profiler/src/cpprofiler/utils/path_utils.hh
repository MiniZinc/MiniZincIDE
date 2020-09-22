#pragma once

#include <vector>
#include <string>


namespace cpprofiler
{

namespace utils {

static constexpr char minor_sep = '|';
static constexpr char major_sep = ';';

struct PathPair {
    std::vector<std::string> model_level;
    std::vector<std::string> decomp_level;
};

PathPair getPathPair(const std::string& path, bool omitDecomp = false);

}
}