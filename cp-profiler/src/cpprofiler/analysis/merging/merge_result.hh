#pragma once

#include "../../core.hh"
#include <vector>

namespace cpprofiler
{
namespace analysis
{

struct PentagonItem
{
    /// pentagon node
    NodeID pen_nid;
    /// left subtree size
    int size_l;
    /// right subtree size
    int size_r;
};

using MergeResult = std::vector<PentagonItem>;

} // namespace analysis
} // namespace cpprofiler