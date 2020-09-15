#include "hide_failed_cursor.hh"
#include "../node_tree.hh"
#include "../visual_flags.hh"
#include "../layout_computer.hh"

namespace cpprofiler
{
namespace tree
{

HideFailedCursor::HideFailedCursor(NodeID start, const NodeTree &nt, VisualFlags &vf, LayoutComputer &lc, bool dirty, bool &mod)
    : NodeCursor(start, nt), m_vf(vf), m_lc(lc), m_onlyDirty(dirty), modified(mod)
{
}

bool HideFailedCursor::mayMoveDownwards() const
{
    const auto node = cur_node();
    bool ok = NodeCursor::mayMoveDownwards() &&
              (!m_onlyDirty || m_lc.isDirty(node)) &&
              (tree_.hasSolvedChildren(node) || tree_.hasOpenChildren(node)) &&
              !m_vf.isHidden(node);

    return ok;
}

inline static bool should_hide(const NodeTree &nt, const VisualFlags &vf, NodeID n)
{
    return !nt.hasSolvedChildren(n) && // does not have solution children
           !nt.hasOpenChildren(n) &&   // does not have open children
           nt.childrenCount(n) > 0 &&  // not a leaf node
           !vf.isHidden(n);            // the node is not already hidden
}

void HideFailedCursor::processCurrentNode()
{
    if (should_hide(tree_, m_vf, cur_node()))
    {
        modified = true;
        m_vf.setHidden(cur_node(), true);
        m_lc.dirtyUpLater(cur_node());
    }
}

} // namespace tree
} // namespace cpprofiler