#ifndef CPPROFILER_TREE_CURSOR_NODE_CURSOR_HH
#define CPPROFILER_TREE_CURSOR_NODE_CURSOR_HH

#include "../node_id.hh"

namespace cpprofiler
{
namespace tree
{

class Structure;
class NodeInfo;
class NodeTree;

class NodeCursor
{

  private:
    /// Current node
    NodeID node_;
    /// Current alternative (position among siblings)
    int cur_alt_;
    /// The root of the tree/subtree
    const NodeID start_node_;

  protected:
    /// Tree Structure
    const NodeTree &tree_;

    /// Return current node
    inline NodeID cur_node() const { return node_; }
    /// Return current alternative
    inline int cur_alt() const { return cur_alt_; }
    /// Return the start (root) node
    inline NodeID start_node() const { return start_node_; }

  public:
    NodeCursor(NodeID start, const NodeTree &tree);

    /// Test if the cursor may move to the parent node
    bool mayMoveUpwards() const;
    /// Move cursor to the parent node
    void moveUpwards();
    /// Test if cursor may move to the first child node
    bool mayMoveDownwards() const;
    /// Move cursor to the first child node
    void moveDownwards();
    /// Test if cursor may move to the first sibling
    bool mayMoveSidewards() const;
    /// Move cursor to the first sibling
    void moveSidewards();
    /// Action performed at the end of traversal
    void finalize();
    /// Action performed per node
    void processCurrentNode();
};

} // namespace tree
} // namespace cpprofiler

#endif