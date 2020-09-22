#ifndef CPPROFILER_TREE_CURSORS_DRAWING_CURSOR_HH
#define CPPROFILER_TREE_CURSORS_DRAWING_CURSOR_HH

#include "node_cursor.hh"
#include "../layout.hh"
#include <QPoint>
#include <QRect>

class QPainter;

namespace cpprofiler
{
class UserData;

namespace tree
{
class VisualFlags;
}
} // namespace cpprofiler

namespace cpprofiler
{
namespace tree
{

class Layout;

/// This uses unsafe methods for tree structure!
class DrawingCursor : public NodeCursor
{

    const Layout &layout_;

    const UserData &user_data_;

    const VisualFlags &vis_flags_;

    const bool debug_mode_;

    QPainter &painter_;
    const QRect clippingRect;

    int cur_x, cur_y;

    bool isClipped();

  public:
    DrawingCursor(NodeID start,
                  const NodeTree &tree,
                  const Layout &layout,
                  const UserData &user_data,
                  const VisualFlags &flags,
                  QPainter &painter,
                  QPoint start_pos,
                  const QRect &clippingRect0,
                  bool debug);

    void processCurrentNode();

    bool mayMoveUpwards();
    bool mayMoveSidewards();
    bool mayMoveDownwards();

    void moveUpwards();
    void moveDownwards();
    void moveSidewards();
};

} // namespace tree
} // namespace cpprofiler

#endif