#pragma once

#include "node_cursor.hh"
#include <QHash>

namespace cpprofiler
{

namespace tree
{

class VisualFlags;
class LayoutComputer;

/// Hide subtrees that are not highlighted
class HideNotHighlightedCursor : public NodeCursor
{
  private:
    QHash<NodeID, bool> onHighlightPath;
    VisualFlags &vf_;
    LayoutComputer &lc_;

  public:
    HideNotHighlightedCursor(NodeID startNode,
                             const NodeTree &tree,
                             VisualFlags &vf,
                             LayoutComputer &lc);
    /// Hide the node if needed
    void processCurrentNode();
    /// Test if cursor may move to the first child node
    bool mayMoveDownwards() const;
};

} // namespace tree
} // namespace cpprofiler