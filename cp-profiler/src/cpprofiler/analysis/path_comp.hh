#include <vector>

#include "../core.hh"

#pragma once

namespace cpprofiler
{
namespace analysis
{

/// Find unique labels on each of the two paths
std::pair<std::vector<Label>, std::vector<Label>>
getLabelDiff(const std::vector<Label> &path1,
             const std::vector<Label> &path2);

} // namespace analysis
} // namespace cpprofiler