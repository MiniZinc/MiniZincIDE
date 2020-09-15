#pragma once

#include "../core.hh"
#include <functional>

using NodeAction = std::function<void(NodeID)>;

namespace cpprofiler
{
namespace tree
{
class NodeTree;
}
} // namespace cpprofiler

namespace cpprofiler
{
namespace utils
{

/// Count all descendants of `n`
int count_descendants(const tree::NodeTree &nt, NodeID n);

/// Compute the node's depth (the distance to the root)
int calculate_depth(const tree::NodeTree &nt, NodeID nid);

/// Apply `action` to `root` and to all of its descendants
void apply_below(const tree::NodeTree &nt, NodeID root, const NodeAction &action);

/// Apply `action` to nodes in the order that corresponds to a pre-order traversal
void pre_order_apply(const tree::NodeTree &nt, NodeID start, const NodeAction &action);

/// Inquire if the node is the right-most child
bool is_right_most_child(const tree::NodeTree &nt, NodeID nid);

/// Return a list of nodes under `nid` (including `nid`)
std::vector<NodeID> nodes_below(const tree::NodeTree &tree, NodeID nid);

/// Return node identifires in an arbitrary order
std::vector<NodeID> any_order(const tree::NodeTree &tree);

/// Return node identifires in the order that corresponds to a pre-order traversal
std::vector<NodeID> pre_order(const tree::NodeTree &tree);

/// Return node identifires in the order that corresponds to a post-order traversal
std::vector<NodeID> post_order(const tree::NodeTree &tree);

/// Calculate subtree sizes for every node in the tree
std::vector<int> calc_subtree_sizes(const tree::NodeTree &tree);

} // namespace utils
} // namespace cpprofiler