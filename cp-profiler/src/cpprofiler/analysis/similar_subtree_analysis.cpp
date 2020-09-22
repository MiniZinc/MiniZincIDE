#include "similar_subtree_analysis.hh"
#include "../tree/node_tree.hh"
#include "../tree/layout.hh"
#include "../utils/tree_utils.hh"
#include "../utils/perf_helper.hh"
#include "../tree/shape.hh"
#include "../tree/visual_flags.hh"
#include "../tree/layout_computer.hh"

#include <set>

namespace cpprofiler
{

namespace analysis
{

using std::vector;
using tree::Layout;
using tree::NodeStatus;
using tree::NodeTree;
using tree::Shape;

struct ShapeInfo
{
    NodeID nid;
    const Shape &shape;
};

struct CompareShapes
{
  public:
    bool operator()(const ShapeInfo &si1, const ShapeInfo &si2) const
    {

        const auto &s1 = si1.shape;
        const auto &s2 = si2.shape;

        if (s1.height() < s2.height())
            return true;
        if (s1.height() > s2.height())
            return false;

        for (int i = 0; i < s1.height(); ++i)
        {
            if (s1[i].l < s2[i].l)
                return false;
            if (s1[i].l > s2[i].l)
                return true;
            if (s1[i].r < s2[i].r)
                return true;
            if (s1[i].r > s2[i].r)
                return false;
        }
        return false;
    }
};

static void set_as_processed(const NodeTree &nt, Partition &partition, int h, std::vector<int> &height_info)
{

    /// Note that in general a group can contain subtrees of
    /// different height; however, since the subtrees of height 'h'
    /// have already been processed, any group that contains such a
    /// subtree is processed and contains only subtrees of height 'h'.
    auto should_stay = [&nt, &height_info, h](const Group &g) {
        auto nid = g.at(0);

        return height_info.at(nid) != h;
    };

    auto &rem = partition.remaining;

    auto first_to_move = std::partition(rem.begin(), rem.end(), should_stay);

    move(first_to_move, rem.end(), std::back_inserter(partition.processed));
    rem.resize(distance(rem.begin(), first_to_move));
}

static vector<Group> separate_marked(const vector<Group> &rem_groups, const vector<NodeID> &marked)
{

    vector<Group> res_groups;

    for (auto &group : rem_groups)
    {

        Group g1; /// marked
        Group g2; /// not marked

        for (auto nid : group)
        {
            if (std::find(marked.begin(), marked.end(), nid) != marked.end())
            {
                g1.push_back(nid);
            }
            else
            {
                g2.push_back(nid);
            }
        }

        if (g1.size() > 0)
        {
            res_groups.push_back(std::move(g1));
        }
        if (g2.size() > 0)
        {
            res_groups.push_back(std::move(g2));
        }
    }

    return res_groups;
}

/// go through processed groups of height 'h' and
/// mark their parent nodes as candidates for separating
static void partition_step(const NodeTree &nt, Partition &p, int h, vector<int> &height_info)
{

    for (auto &group : p.processed)
    {

        /// Check if the group should be skipped
        /// Note that processed nodes are assumed to be of the same height
        if (group.size() == 0 || height_info.at(group[0]) != h)
        {
            continue;
        }

        const auto max_kids = std::accumulate(group.begin(), group.end(), 0, [&nt](int cur, NodeID nid) {
            auto pid = nt.getParent(nid);
            return std::max(cur, nt.childrenCount(pid));
        });

        int alt = 0;
        for (auto alt = 0; alt < max_kids; ++alt)
        {

            vector<NodeID> marked;

            for (auto nid : group)
            {
                if (nt.getAlternative(nid) == alt)
                {
                    /// Mark the parent of nid to separate
                    auto pid = nt.getParent(nid);
                    marked.push_back(pid);
                }
            }

            /// separate marked nodes from their groups
            p.remaining = separate_marked(p.remaining, marked);
        }
    }
}

static Partition initialPartition(const NodeTree &nt)
{
    /// separate leaf nodes from internal
    /// NOTE: this assumes that branch nodes have at least one child!
    Group failed_nodes;
    Group solution_nodes;
    Group branch_nodes;
    {
        auto nodes = utils::any_order(nt);

        for (auto nid : nodes)
        {
            auto status = nt.getStatus(nid);
            switch (status)
            {
            case NodeStatus::FAILED:
            {
                failed_nodes.push_back(nid);
            }
            break;
            case NodeStatus::SOLVED:
            {
                solution_nodes.push_back(nid);
            }
            break;
            case NodeStatus::BRANCH:
            {
                branch_nodes.push_back(nid);
            }
            break;
            default:
            {
                /// ignore other nodes for now
            }
            break;
            }
        }
    }

    Partition result{{}, {}};

    if (failed_nodes.size() > 0)
    {
        result.processed.push_back(std::move(failed_nodes));
    }

    if (solution_nodes.size() > 0)
    {
        result.processed.push_back(std::move(solution_nodes));
    }

    if (branch_nodes.size() > 0)
    {
        result.remaining.push_back(std::move(branch_nodes));
    }

    return result;
}

static int calculateHeightOf(NodeID nid, const NodeTree &nt, vector<int> &height_info)
{

    int cur_max = 0;

    auto kids = nt.childrenCount(nid);

    if (kids == 0)
    {
        cur_max = 1;
    }
    else
    {
        for (auto alt = 0; alt < nt.childrenCount(nid); ++alt)
        {
            auto child = nt.getChild(nid, alt);
            cur_max = std::max(cur_max, calculateHeightOf(child, nt, height_info) + 1);
        }
    }

    height_info[nid] = cur_max;

    return cur_max;
}

/// TODO: make sure the structure isn't changing anymore
/// TODO: this does not work correctly for n-ary trees yet
vector<SubtreePattern> runIdenticalSubtrees(const NodeTree &nt)
{

    auto label_opt = LabelOption::IGNORE_LABEL;

    vector<int> height_info(nt.nodeCount());

    auto max_height = calculateHeightOf(nt.getRoot(), nt, height_info);

    auto max_depth = nt.depth();

    auto cur_height = 1;

    auto sizes = utils::calc_subtree_sizes(nt);

    // 0) Initial Partition
    auto partition = initialPartition(nt);

    while (cur_height != max_height)
    {

        partition_step(nt, partition, cur_height, height_info);

        /// By this point, all subtrees of height 'cur_height + 1'
        /// should have been processed
        set_as_processed(nt, partition, cur_height + 1, height_info);

        ++cur_height;
    }

    /// Construct the result in the appropriate form
    vector<SubtreePattern> result;
    result.reserve(partition.processed.size());

    for (auto &&group : partition.processed)
    {

        if (group.empty())
            continue;

        auto height = height_info.at(group[0]);

        const auto first = group[0];

        SubtreePattern pattern(height);

        pattern.m_nodes = std::move(group);
        pattern.setSize(sizes.at(first));

        result.push_back(std::move(pattern));
    }

    return result;
}

/// SIMILAR SHAPE ANALYSIS

std::vector<SubtreePattern> runSimilarShapes(const NodeTree &tree, const Layout &lo)
{

    std::multiset<ShapeInfo, CompareShapes> shape_set;

    auto node_order = utils::any_order(tree);

    for (const auto nid : node_order)
    {
        shape_set.insert({nid, *lo.getShape(nid)});
    }

    auto sizes = utils::calc_subtree_sizes(tree);

    std::vector<SubtreePattern> shapes;

    auto it = shape_set.begin();
    auto end = shape_set.end();

    while (it != end)
    {
        auto upper = shape_set.upper_bound(*it);

        const int height = it->shape.height();

        SubtreePattern pattern(height);
        for (; it != upper; ++it)
        {
            pattern.m_nodes.emplace_back(it->nid);
        }

        pattern.setSize(sizes.at(pattern.first()));

        shapes.push_back(std::move(pattern));
    }

    return shapes;
}

} // namespace analysis

} // namespace cpprofiler