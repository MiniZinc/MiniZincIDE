#include "tree_scroll_area.hh"

#include <QPainter>
#include <QScrollBar>
#include <QMouseEvent>

#include <cmath>
#include <stack>
#include <queue>
#include <thread>

#include "shape.hh"
#include "node_tree.hh"
#include "visual_flags.hh"
#include "cursors/nodevisitor.hh"
#include "cursors/drawing_cursor.hh"

#include "../utils/perf_helper.hh"
#include "../utils/utils.hh"
#include "../config.hh"

namespace cpprofiler
{
namespace tree
{

constexpr int y_margin = 20;

static void drawGrid(QPainter &painter, QSize size)
{

    const int cell_w = 100;
    const int cell_h = 100;

    {
        QPen pen = painter.pen();
        pen.setWidth(1);
        pen.setStyle(Qt::DotLine);
        painter.setPen(pen);
    }

    const auto columns = std::ceil(static_cast<float>(size.width()) / cell_w);
    const auto rows = std::ceil(static_cast<float>(size.height()) / cell_h);

    for (auto row = 0; row < rows; ++row)
    {
        for (auto col = 0; col < columns; ++col)
        {
            auto x = col * cell_w;
            auto y = row * cell_h;
            painter.drawRect(x, y, cell_w, cell_w);
        }
    }
}

void TreeScrollArea::paintEvent(QPaintEvent *event)
{

    QPainter painter(this->viewport());

    painter.setRenderHint(QPainter::Antialiasing);

    if (m_start_node == NodeID::NoNode)
    {
        return;
    }

    painter.scale(m_options.scale, m_options.scale);

    if (!m_layout.ready(m_start_node) || !m_layout.getLayoutDone(m_start_node))
    {
        return;
    }

    auto bb = m_layout.getBoundingBox(m_start_node);

    auto tree_width = bb.right - bb.left;

    auto tree_height = m_layout.getHeight(m_start_node) * layout::dist_y;

    auto viewport_size = viewport()->size();

    auto h_page_step = viewport_size.width() / m_options.scale;
    auto v_page_step = viewport_size.height() / m_options.scale;

    /// set range in real pixels
    horizontalScrollBar()->setRange(0, tree_width - h_page_step);
    horizontalScrollBar()->setPageStep(h_page_step);

    verticalScrollBar()->setRange(0, tree_height + y_margin / m_options.scale - v_page_step);
    verticalScrollBar()->setPageStep(v_page_step);

    horizontalScrollBar()->setSingleStep(layout::min_dist_x);
    verticalScrollBar()->setSingleStep(layout::dist_y);

    {
        QPen pen = painter.pen();
        pen.setWidth(2);
        painter.setPen(pen);
    }

    auto x_off = horizontalScrollBar()->value();
    auto y_off = verticalScrollBar()->value();
    painter.translate(-x_off, -y_off);
    /// TODO: this need to aquire layout mutex

    auto half_w = viewport()->width() / 2;
    auto half_h = viewport()->height() / 2;

    auto displayed_width = static_cast<int>(viewport()->size().width() / m_options.scale);
    auto displayed_height = static_cast<int>(viewport()->size().height() / m_options.scale);

    // auto start_x = m_options.x_off + displayed_width / 2;

    if (displayed_width > tree_width)
    {
        m_options.root_x = (displayed_width - tree_width) / 2 - bb.left;
    }
    else
    {
        m_options.root_x = -bb.left;
    }

    m_options.root_y = y_margin / m_options.scale;
    auto start_pos = QPoint{m_options.root_x, m_options.root_y};

    const int clip_margin = 0 / m_options.scale;

    QRect clip{QPoint{x_off + clip_margin, y_off + clip_margin},
               QSize{displayed_width - 2 * clip_margin, displayed_height - 2 * clip_margin}};

    painter.drawRect(clip);

    // drawGrid(painter, {std::max(tree_width, displayed_width), std::max(tree_height, displayed_height)});

    utils::MutexLocker tree_locker(&m_tree.treeMutex());

    DrawingCursor dc(m_start_node, m_tree, m_layout, user_data_, m_vis_flags, painter, start_pos, clip, debug_mode_);
    PreorderNodeVisitor<DrawingCursor>(dc).run();
}

QPoint TreeScrollArea::getNodeCoordinate(NodeID nid)
{
    auto node = nid;
    auto pos = QPoint{m_options.root_x, m_options.root_y};

    while (node != m_tree.getRoot())
    {
        pos += {static_cast<int>(m_layout.getOffset(node)), layout::dist_y};
        node = m_tree.getParent(node);
    }

    return pos;
}

static std::pair<int, int> getRealBB(NodeID nid, const NodeTree &tree, const Layout &layout, const DisplayState &ds)
{

    auto bb = layout.getBoundingBox(nid);

    auto node = nid;
    while (node != tree.getRoot())
    {
        bb.left += layout.getOffset(node);
        bb.right += layout.getOffset(node);
        node = tree.getParent(node);
    }

    bb.left += ds.root_x;
    bb.right += ds.root_x;

    return {bb.left, bb.right};
}

void TreeScrollArea::setScale(int val)
{
    m_options.scale = val / 50.0f;
    viewport()->update();
}

/// Make sure the layout for nodes is done
NodeID TreeScrollArea::findNodeClicked(int x, int y)
{
    utils::MutexLocker tree_lock(&m_tree.treeMutex());
    utils::MutexLocker layout_lock(&m_layout.getMutex());

    using namespace traditional;

    /// TODO: disable while constructing
    /// calculate real x and y
    auto x_off = horizontalScrollBar()->value();
    auto y_off = verticalScrollBar()->value();

    x = x / m_options.scale + x_off;
    y = y / m_options.scale + y_off;

    std::queue<NodeID> queue;

    auto root = m_tree.getRoot();

    if (root == NodeID::NoNode)
    {
        return NodeID::NoNode;
    }

    queue.push(root);

    while (!queue.empty())
    {

        auto node = queue.front();
        queue.pop();
        auto node_pos = getNodeCoordinate(node);

        const auto hidden = m_vis_flags.isHidden(node);

        QRect node_area;
        if (hidden)
        {
            auto node_pos_tl = node_pos - QPoint{HALF_COLLAPSED_WIDTH, 0};
            node_area = QRect(node_pos_tl, QSize{COLLAPSED_WIDTH, COLLAPSED_DEPTH});
        }
        else
        {
            auto node_pos_tl = node_pos - QPoint{MAX_NODE_W / 2, 0};
            node_area = QRect(node_pos_tl, QSize{MAX_NODE_W, MAX_NODE_W});
        }

        if (node_area.contains(x, y))
        {
            return node;
        }
        else if (!hidden)
        {

            auto kids = m_tree.childrenCount(node);

            for (auto i = 0; i < kids; ++i)
            {
                auto kid = m_tree.getChild(node, i);

                /// the kid does not have bounding box yet
                if (!m_layout.getLayoutDone(kid))
                    continue;

                auto pair = getRealBB(kid, m_tree, m_layout, m_options);
                if (pair.first <= x && pair.second >= x)
                {
                    queue.push(kid);
                }
            }
        }
    }

    return NodeID::NoNode;
}

void TreeScrollArea::mousePressEvent(QMouseEvent *me)
{
    auto n = findNodeClicked(me->x(), me->y());
    if (n != NodeID::NoNode)
    {
        emit nodeClicked(n);
    }
}

void TreeScrollArea::mouseDoubleClickEvent(QMouseEvent *me)
{
    auto n = findNodeClicked(me->x(), me->y());
    if (n != NodeID::NoNode)
    {
        emit nodeDoubleClicked(n);
    }
}

TreeScrollArea::TreeScrollArea(NodeID start, const NodeTree &tree, const UserData &user_data, const Layout &layout, const VisualFlags &nf)
    : m_start_node(start), m_tree(tree), user_data_(user_data), m_layout(layout), m_vis_flags(nf)
{
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
}

void TreeScrollArea::centerPoint(int x, int y)
{
    const auto viewport_size = viewport()->size();
    const auto h_page_step = viewport_size.width() / m_options.scale;
    const auto v_page_step = viewport_size.height() / m_options.scale;

    const auto value_x = std::max(0, static_cast<int>(x - h_page_step / 2));
    horizontalScrollBar()->setValue(value_x);

    const auto value_y = std::max(0, static_cast<int>(y - v_page_step / 2));
    verticalScrollBar()->setValue(value_y);
}

void TreeScrollArea::changeStartNode(NodeID nid)
{
    m_start_node = nid;
}

} // namespace tree
} // namespace cpprofiler