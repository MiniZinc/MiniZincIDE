#include "node_cursor.hh"

#include "../structure.hh"
#include "../node_tree.hh"
#include <QDebug>

namespace cpprofiler
{
namespace tree
{

NodeCursor::NodeCursor(NodeID start, const NodeTree &tree)
    : tree_(tree), start_node_(start), node_(start), cur_alt_(0)
{
}

bool NodeCursor::mayMoveDownwards() const
{
    auto n = tree_.childrenCount(node_);
    return (n > 0);
}

void NodeCursor::moveDownwards()
{
    cur_alt_ = 0;
    node_ = tree_.getChild(node_, 0);
}

bool NodeCursor::mayMoveSidewards() const
{
    return (node_ != start_node_) &&
           (node_ != tree_.getRoot()) &&
           (cur_alt_ < tree_.getNumberOfSiblings(node_) - 1);
}

void NodeCursor::moveSidewards()
{
    auto parent_nid = tree_.getParent(node_);
    node_ = tree_.getChild(parent_nid, ++cur_alt_);
}

bool NodeCursor::mayMoveUpwards() const
{
    return (node_ != start_node_) && (node_ != tree_.getRoot());
}

void NodeCursor::moveUpwards()
{
    node_ = tree_.getParent(node_);
    cur_alt_ = tree_.getAlternative(node_);
}

void NodeCursor::finalize()
{
    /// do nothing
}

} // namespace tree
} // namespace cpprofiler