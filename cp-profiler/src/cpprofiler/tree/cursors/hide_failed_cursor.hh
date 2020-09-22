#pragma once

#include "node_cursor.hh"

namespace cpprofiler
{
namespace tree
{

class VisualFlags;
class LayoutComputer;

/// To be used with post-order visitor to hide the
/// largest subtrees that contain no solutions
class HideFailedCursor : public NodeCursor
{

  VisualFlags &m_vf;
  LayoutComputer &m_lc;

  /// Whether only dirty nodes should be hidden
  bool m_onlyDirty;
  /// Whether a the cursor modified the visualisation in any way
  bool &modified;

public:
  HideFailedCursor(NodeID start,       // The root of the tree/subtree
                   const NodeTree &nt, // Tree structure
                   VisualFlags &vf,    // Visual flags (whether a node is hidden etc.)
                   LayoutComputer &lc, // Class for computing the layout
                   bool onlyDirty,     // Whether non-dirty nodes should be visited
                   bool &mod);         // out-parameter to indicate if the visualisation was modified

  /// Test if the cursor may move to the first child node
  bool mayMoveDownwards() const;
  /// Process node
  void processCurrentNode();
};

} // namespace tree
} // namespace cpprofiler