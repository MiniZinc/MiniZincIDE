#pragma once

#include <vector>
#include "../core.hh"
#include "subtree_pattern.hh"

namespace cpprofiler
{

namespace tree
{
class NodeTree;
class Layout;
} // namespace tree

namespace analysis
{

using Group = std::vector<NodeID>;

struct Partition
{

    std::vector<Group> processed;
    std::vector<Group> remaining;

    Partition(std::vector<Group> &&proc, std::vector<Group> &&rem)
        : processed(proc), remaining(rem)
    {
    }
};

enum class LabelOption
{
    IGNORE_LABEL,
    VARS,
    FULL
};

struct SubtreePattern;

std::vector<SubtreePattern> runIdenticalSubtrees(const tree::NodeTree &nt);

std::vector<SubtreePattern> runSimilarShapes(const tree::NodeTree &tree, const tree::Layout &lo);

} // namespace analysis

} // namespace cpprofiler
