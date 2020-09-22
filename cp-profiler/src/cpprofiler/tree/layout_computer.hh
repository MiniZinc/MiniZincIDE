#ifndef CPPROFILER_TREE_LAYOUT_COMPUTER_HH
#define CPPROFILER_TREE_LAYOUT_COMPUTER_HH

class QMutex;

#include "node_id.hh"

#include <set>
#include <vector>

namespace cpprofiler
{
namespace tree
{

class Layout;
class NodeTree;
class VisualFlags;

class LayoutComputer
{

    const NodeTree &m_tree;
    const VisualFlags &m_vis_flags;
    Layout &m_layout;

    bool m_needs_update = true;

    bool debug_mode_ = false;

    /// Nodes to dirty up right before next layout update
    std::set<NodeID> du_node_set_;

    void dirtyUp(NodeID nid);

  public:
    LayoutComputer(const NodeTree &tree, Layout &layout, const VisualFlags &nf);

    /// compute the layout and return where any work was required
    bool compute();

    /// Mark node's ancestors as dirty without stopping at an already dirty node
    void dirtyUpUnconditional(NodeID nid);

    void dirtyUpLater(NodeID nid);

    bool isDirty(NodeID nid);

    void setDirty(NodeID nid);

    void setDebugMode(bool val) { debug_mode_ = val; }
};

} // namespace tree
} // namespace cpprofiler

#endif