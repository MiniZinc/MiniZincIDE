#include "drawing_cursor.hh"
#include "../../config.hh"
#include "../layout.hh"
#include "../node_info.hh"
#include "../node.hh"
#include "../shape.hh"
#include "../../user_data.hh"
#include "../../utils/tree_utils.hh"
#include "../traditional_view.hh"

#include "../node_drawing.hh"

#include "../node_tree.hh"

#include <QDebug>
#include <QPainter>

namespace cpprofiler
{
namespace tree
{

/// remove this copy
namespace colors
{
/// The color for selected nodes
static QColor gold(252, 209, 22);
/// Red color for failed nodes
static QColor red(218, 37, 29);
/// Green color for solved nodes
static QColor green(11, 118, 70);
/// Blue color for choice nodes
static QColor blue(0, 92, 161);
/// Grey color for skipped nodes
static QColor grey(150, 150, 150);
/// Orange color for best solutions
static QColor orange(235, 137, 27);
/// White color
static QColor white(255, 255, 255);

/// Red color for expanded failed nodes
static QColor lightRed(218, 37, 29, 120);
/// Green color for expanded solved nodes
static QColor lightGreen(11, 118, 70, 120);
/// Blue color for expanded choice nodes
static QColor lightBlue(0, 92, 161, 120);
} // namespace colors

DrawingCursor::DrawingCursor(NodeID start,
                             const NodeTree &tree,
                             const Layout &layout,
                             const UserData &user_data,
                             const VisualFlags &flags,
                             QPainter &painter,
                             QPoint start_pos,
                             const QRect &clip,
                             bool debug)
    : NodeCursor(start, tree),
      layout_(layout),
      user_data_(user_data),
      vis_flags_(flags),
      painter_(painter),
      clippingRect(clip),
      debug_mode_(debug)
{
    cur_x = start_pos.x();
    cur_y = start_pos.y();
}

static void drawTriangle(QPainter &painter, int x, int y, bool selected, bool has_gradient, bool has_solutions)
{
    using namespace traditional;

    if (selected)
    {
        painter.setBrush(colors::gold);
    }
    else
    {
        QColor main_color = has_solutions ? colors::green : colors::red;
        if (has_gradient)
        {
            QLinearGradient gradient(x - COLLAPSED_WIDTH, y,
                                     x + COLLAPSED_WIDTH * 1.3, y + COLLAPSED_DEPTH * 1.3);
            gradient.setColorAt(0, colors::white);
            gradient.setColorAt(1, main_color);
            painter.setBrush(gradient);
        }
        else
        {
            painter.setBrush(main_color);
        }
    }

    QPointF points[3] = {QPointF(x, y),
                         QPointF(x + HALF_COLLAPSED_WIDTH, y + COLLAPSED_DEPTH),
                         QPointF(x - HALF_COLLAPSED_WIDTH, y + COLLAPSED_DEPTH)};

    painter.drawConvexPolygon(points, 3);
}

static void drawShape(QPainter &painter, int x, int y, NodeID nid, const Layout &layout)
{
    using namespace traditional;

    auto old_pen = painter.pen();
    auto old_brush = painter.brush();

    painter.setBrush(QColor{0, 0, 0, 50});
    painter.setPen(Qt::NoPen);

    const auto &shape = *layout.getShape(nid);

    const int height = shape.height();
    QPointF *points = new QPointF[height * 2];

    int l_x = x + shape[0].l;
    int r_x = x + shape[0].r;
    y = y + BRANCH_WIDTH / 2;

    points[0] = QPointF(l_x, y);
    points[height * 2 - 1] = QPointF(r_x, y);

    for (int i = 1; i < height; i++)
    {
        y += static_cast<double>(layout::dist_y);
        l_x = x + shape[i].l;
        r_x = x + shape[i].r;
        points[i] = QPointF(l_x, y);
        points[height * 2 - i - 1] = QPointF(r_x, y);
    }

    painter.drawConvexPolygon(points, shape.height() * 2);

    delete[] points;

    painter.setPen(old_pen);
    painter.setBrush(old_brush);
}

static void drawBoundingBox(QPainter &painter, int x, int y, NodeID nid, const Layout &layout)
{
    auto bb = layout.getBoundingBox(nid);
    auto height = layout.getHeight(nid) * layout::dist_y;
    painter.drawRect(x + bb.left, y, bb.right - bb.left, height);
}

void DrawingCursor::processCurrentNode()
{
    using namespace traditional;

    bool phantom_node = false;

    painter_.setPen(QColor{Qt::black});

    const auto node = cur_node();

    if (node != start_node())
    {
        auto parent_x = cur_x - layout_.getOffset(node);
        auto parent_y = cur_y - static_cast<double>(layout::dist_y);

        painter_.drawLine(parent_x, parent_y + BRANCH_WIDTH, cur_x, cur_y);
    }

    auto status = tree_.getStatus(node);

    /// NOTE: this should be consisten with the layout
    if (vis_flags_.isLabelShown(node))
    {

        auto draw_left = !utils::is_right_most_child(tree_, node);
        // painter_.setPen(QPen{Qt::black, 2});
        const Label &label = debug_mode_ ? std::to_string(node) : tree_.getLabel(node);

        auto fm = painter_.fontMetrics();
        auto label_width = fm.width(label.c_str());

        {
            auto font = painter_.font();
            font.setStyleHint(QFont::Monospace);
            painter_.setFont(font);
        }

        int label_x;
        if (draw_left)
        {
            label_x = cur_x - HALF_MAX_NODE_W - label_width;
        }
        else
        {
            label_x = cur_x + HALF_MAX_NODE_W;
        }

        painter_.drawText(QPoint{label_x, cur_y}, label.c_str());
    }

    if (vis_flags_.isHighlighted(node))
    {
        drawShape(painter_, cur_x, cur_y, node, layout_);
    }

    const auto sel_node = user_data_.getSelectedNode();
    const auto selected = (sel_node == node) ? true : false;

    // if (selected)
    // {
    //     painter_.setBrush(QColor{0, 0, 0, 20});

    //     drawBoundingBox(painter_, cur_x, cur_y, node, layout_);

    //     drawShape(painter_, cur_x, cur_y, node, layout_);
    // }

    /// see if the node is hidden

    auto hidden = vis_flags_.isHidden(node);

    if (hidden)
    {

        if (status == NodeStatus::MERGED)
        {
            draw::big_pentagon(painter_, cur_x, cur_y, selected);
            return;
        }

        const bool has_gradient = tree_.hasOpenChildren(node);
        const bool has_solutions = tree_.hasSolvedChildren(node);

        /// check if the node is a lantern node
        const auto lantern_size = vis_flags_.lanternSize(node);
        if (lantern_size == -1)
        {

            drawTriangle(painter_, cur_x, cur_y, selected, has_gradient, has_solutions);
        }
        else
        {
            draw::lantern(painter_, cur_x, cur_y, lantern_size, selected, has_gradient, has_solutions);
        }

        return;
    }

    switch (status)
    {
    case NodeStatus::SOLVED:
    {
        draw::solution(painter_, cur_x, cur_y, selected);
    }
    break;
    case NodeStatus::FAILED:
    {
        draw::failure(painter_, cur_x, cur_y, selected);
    }
    break;
    case NodeStatus::BRANCH:
    {
        draw::branch(painter_, cur_x, cur_y, selected);
    }
    break;
    case NodeStatus::SKIPPED:
    {
        draw::skipped(painter_, cur_x, cur_y, selected);
    }
    break;
    case NodeStatus::MERGED:
    {
        draw::pentagon(painter_, cur_x, cur_y, selected);
    }
    break;
    default:
    {
        draw::unexplored(painter_, cur_x, cur_y, selected);
    }
    break;
    }

    if (user_data_.isBookmarked(node))
    {
        painter_.setBrush(Qt::black);
        painter_.drawEllipse(cur_x - 10, cur_y, 10.0, 10.0);
    }
}

void DrawingCursor::moveUpwards()
{
    cur_x -= layout_.getOffset(cur_node());
    cur_y -= layout::dist_y;
    NodeCursor::moveUpwards();
}

void DrawingCursor::moveDownwards()
{
    NodeCursor::moveDownwards();
    cur_x += layout_.getOffset(cur_node());
    cur_y += layout::dist_y;
}

void DrawingCursor::moveSidewards()
{
    cur_x -= layout_.getOffset(cur_node());
    NodeCursor::moveSidewards();
    cur_x += layout_.getOffset(cur_node());
}

bool DrawingCursor::mayMoveSidewards()
{
    /// whether the next node is present
    const auto may_move = NodeCursor::mayMoveSidewards();

    if (!may_move)
        return false;

    const auto parent = tree_.getParent(cur_node());
    const auto alt = tree_.getAlternative(cur_node());
    const auto next = tree_.getChild(parent, alt + 1);

    /// wether the node is ready for drawing
    const auto next_layout_done = layout_.getLayoutDone(next);

    return next_layout_done;
}

bool DrawingCursor::mayMoveDownwards()
{

    const auto has_children = (tree_.childrenCount(cur_node()) > 0);
    if (!has_children)
        return false;

    const auto clipped = isClipped();
    if (clipped)
        return false;

    const auto hidden = vis_flags_.isHidden(cur_node());

    if (hidden)
        return false;

    const auto kid = tree_.getChild(cur_node(), 0);
    const auto kid_layout_done = layout_.getLayoutDone(kid);

    return kid_layout_done;
}

bool DrawingCursor::mayMoveUpwards()
{
    return NodeCursor::mayMoveUpwards();
}

bool DrawingCursor::isClipped()
{
    const auto bb = layout_.getBoundingBox(cur_node());

    if (
        (cur_x + bb.left > clippingRect.x() + clippingRect.width()) ||
        (cur_x + bb.right < clippingRect.x()) ||
        (cur_y > clippingRect.y() + clippingRect.height()) ||
        (cur_y + (layout_.getHeight(cur_node()) + 1) * layout::dist_y < clippingRect.y()))
    {
        return true;
    }

    return false;
}

} // namespace tree
} // namespace cpprofiler