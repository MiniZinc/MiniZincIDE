
#include "layout_cursor.hh"
#include "../../config.hh"
#include "../node_id.hh"
#include <iostream>
#include <QDebug>
#include <QPainter>

#include "../layout.hh"
#include "../node_tree.hh"
#include "../structure.hh"
#include "../shape.hh"
#include "../../config.hh"
#include "../../utils/tree_utils.hh"
#include "../../utils/debug.hh"

/// needed for VisualFlags
#include "../traditional_view.hh"

#include <numeric>
#include <climits>
#include <cmath>

namespace cpprofiler
{
namespace tree
{

LayoutCursor::LayoutCursor(NodeID start, const NodeTree &tree, const VisualFlags &nf, Layout &lo, bool debug)
    : NodeCursor(start, tree), m_layout(lo), tree_(tree), m_vis_flags(nf), debug_mode_(debug) {}

bool LayoutCursor::mayMoveDownwards()
{
    const auto has_children = NodeCursor::mayMoveDownwards();
    const auto hidden = m_vis_flags.isHidden(cur_node());
    const auto dirty = m_layout.isDirty(cur_node());
    const auto layout_done = m_layout.getLayoutDone(cur_node());

    const auto mayMoveDown = has_children && !hidden && dirty;

    return mayMoveDown;

    // return NodeCursor::mayMoveDownwards() &&
    //        (!m_vis_flags.isHidden(cur_node())) &&
    //        m_layout.isDirty(cur_node());
}

/// Compute the distance between s1 and s2 (that is, how far apart should the
/// corresponding root nodes be along X axis to avoid overlap, additionally
/// allowing for some margin -- layout::min_dist_x)
static int distance_between(const Shape &s1, const Shape &s2)
{
    const auto common_depth = std::min(s1.height(), s2.height());

    auto max = INT_MIN;
    for (auto i = 0; i < common_depth; ++i)
    {
        auto cur_dist = s1[i].r - s2[i].l;
        if (cur_dist > max)
            max = cur_dist;
    }

    return max + layout::min_dist_x;
}

/// Combine shapes s1 and s2 to form a shape of a shape for the parent node;
/// offsets will contain the resulting relative distance from the parent node (along x);
static ShapeUniqPtr combine_shapes(const Shape &s1, const Shape &s2, std::vector<int> &offsets)
{

    const auto depth_left = s1.height();
    const auto depth_right = s2.height();

    const auto max_depth = std::max(depth_left, depth_right);
    const auto common_depth = std::min(depth_left, depth_right);

    const auto distance = distance_between(s1, s2);
    const auto half_dist = distance / 2;

    auto combined = ShapeUniqPtr(new Shape{max_depth + 1});

    {
        const auto bb_left = std::min(s1.boundingBox().left - half_dist,
                                      s2.boundingBox().left + half_dist);
        const auto bb_right = std::max(s1.boundingBox().right - half_dist,
                                       s2.boundingBox().right + half_dist);
        combined->setBoundingBox(BoundingBox{bb_left, bb_right});
    }

    offsets[0] = -half_dist;
    offsets[1] = half_dist;

    /// Calculate extents for levels shared by both shapes
    for (auto depth = 0; depth < common_depth; ++depth)
    {
        (*combined)[depth + 1] = {s1[depth].l - half_dist, s2[depth].r + half_dist};
    }

    /// Calculate extents for levels where only one subtree has nodes
    if (max_depth != common_depth)
    {
        const auto &longer_shape = depth_left > depth_right ? s1 : s2;
        const int offset = depth_left > depth_right ? -half_dist : half_dist;

        for (auto depth = common_depth; depth < max_depth; ++depth)
        {
            (*combined)[depth + 1] = {longer_shape[depth].l + offset,
                                      longer_shape[depth].r + offset};
        }
    }

    return std::move(combined);
}

/// Merge two "sibling" shapes, without adding a new depth level;
/// `dist` is the disntance between s2 and the left extent
static Shape merge_left(const Shape &s1, const Shape &s2, int &dist)
{

    const auto depth_left = s1.height();
    const auto depth_right = s2.height();

    const auto common_depth = std::min(depth_left, depth_right);
    const auto max_depth = std::max(depth_left, depth_right);

    const auto distance = distance_between(s1, s2);

    dist = distance - s1.boundingBox().left;

    Shape result{max_depth};

    /// compute bounding box
    {
        auto bb_left = std::min(s1.boundingBox().left,
                                s2.boundingBox().left + distance);
        auto bb_right = std::max(s1.boundingBox().right,
                                 s2.boundingBox().right + distance);
        result.setBoundingBox(BoundingBox{bb_left, bb_right});
    }

    /// Calculate extents for levels shared by both shapes
    for (auto depth = 0; depth < common_depth; ++depth)
    {
        result[depth] = {s1[depth].l, s2[depth].r + distance};
    }

    /// Calculate extents for levels where only one subtree has nodes
    if (max_depth != common_depth)
    {
        const auto &longer_shape = depth_left > depth_right ? s1 : s2;
        const int offset = depth_left > depth_right ? 0 : distance;

        for (auto depth = common_depth; depth < max_depth; ++depth)
        {
            result[depth] = {longer_shape[depth].l + offset,
                             longer_shape[depth].r + offset};
        }
    }

    return result;
}

static Extent calculateForSingleNode(NodeID nid, const NodeTree &nt, bool label_shown, bool hidden, bool debug)
{

    Extent result{-traditional::HALF_MAX_NODE_W, traditional::HALF_MAX_NODE_W};
    /// see if the node dispays labels and needs its (top) extents extended

    if (hidden)
    {
        result = {-traditional::HALF_COLLAPSED_WIDTH, traditional::HALF_COLLAPSED_WIDTH};
    }

    if (!label_shown)
        return result;

    /// TODO: use font metrics?
    const auto &label = debug ? std::to_string(nid) : nt.getLabel(nid);
    auto label_width = label.size() * 9;

    /// Note: this assumes that the default painter used for drawing text
    // QPainter painter;
    // auto fm = painter.fontMetrics();
    // auto label_width = fm.width(label.c_str());

    /// Note that labels are shown on the left for all alt
    /// except the last one (right-most)

    bool draw_left = !utils::is_right_most_child(nt, nid);

    if (draw_left)
    {
        result.l -= label_width;
    }
    else
    {
        result.r += label_width;
    }
    return result;
}

inline static void computeForNodeBinary(NodeID nid, Layout &layout, const NodeTree &nt, bool label_shown, bool debug)
{

    auto kid_l = nt.getChild(nid, 0);
    auto kid_r = nt.getChild(nid, 1);

    const auto &s1 = *layout.getShape(kid_l);
    const auto &s2 = *layout.getShape(kid_r);

    std::vector<int> offsets(2);
    auto combined = combine_shapes(s1, s2, offsets);

    (*combined)[0] = calculateForSingleNode(nid, nt, label_shown, false, debug);

    /// Extents for root node changed -> check if bounding box is correct
    const auto &bb = combined->boundingBox();

    if (bb.left > (*combined)[0].l || bb.right < (*combined)[0].r)
    {
        combined->setBoundingBox({std::min(bb.left, (*combined)[0].l), std::max(bb.right, (*combined)[0].r)});
    }

    layout.setShape(nid, std::move(combined));

    layout.setChildOffset(kid_l, offsets[0]);
    layout.setChildOffset(kid_r, offsets[1]);
}

inline static std::vector<int> compute_distances(NodeID nid, int nkids, Layout &lo, const NodeTree &tree)
{
    std::vector<int> distances(nkids - 1);

    for (auto i = 0; i < nkids - 1; ++i)
    {
        auto kid_l = tree.getChild(nid, i);
        auto kid_r = tree.getChild(nid, i + 1);
        const auto &s1 = *lo.getShape(kid_l);
        const auto &s2 = *lo.getShape(kid_r);
        distances[i] = distance_between(s1, s2);
    }

    return distances;
}

inline static int kids_max_depth(NodeID nid, int nkids, const Layout &lo, const NodeTree &tree)
{
    int max_depth = 0;
    for (auto i = 0; i < nkids; ++i)
    {
        auto kid = tree.getChild(nid, i);
        const auto &s1 = *lo.getShape(kid);
        max_depth = std::max(max_depth, s1.height());
    }
    return max_depth;
}

static inline void computeForNodeNary(NodeID nid, int nkids, Layout &layout, const NodeTree &tree, bool debug)
{

    /// calculate all distances
    const auto distances = compute_distances(nid, nkids, layout, tree);

    /// distance between the leftmost and the rightmost nodes
    int max_dist = 0;
    for (auto distance : distances)
    {
        max_dist += distance;
    }

    /// calculate the depth of the resulting shape
    const auto new_depth = kids_max_depth(nid, nkids, layout, tree) + 1;

    auto combined = ShapeUniqPtr(new Shape{new_depth});

    std::vector<int> x_offsets(nkids);
    /// calculate offsets
    auto cur_x = -max_dist / 2;
    for (auto i = 0; i < nkids; ++i)
    {
        const auto kid = tree.getChild(nid, i);
        layout.setChildOffset(kid, cur_x);
        x_offsets[i] = cur_x;
        if (i < nkids - 1) {
            cur_x += distances[i];
        }
    }

    /// calculate extents
    /// TODO: does this need to take labels into account?
    (*combined)[0] = {-traditional::HALF_MAX_NODE_W, traditional::HALF_MAX_NODE_W};
    for (auto depth = 1; depth < new_depth; ++depth)
    {

        auto leftmost_x = INT_MAX;
        auto rightmost_x = INT_MIN;
        for (auto alt = 0; alt < nkids; ++alt)
        {
            const auto kid = tree.getChild(nid, alt);
            const auto &shape = *layout.getShape(kid);
            if (shape.height() > depth - 1)
            {
                leftmost_x = std::min(leftmost_x, shape[depth - 1].l + x_offsets[alt]);
                rightmost_x = std::max(leftmost_x, shape[depth - 1].r + x_offsets[alt]);
            }
        }

        (*combined)[depth].l = leftmost_x;
        (*combined)[depth].r = rightmost_x;
    }

    /// calculate bounding box
    int l_bound = INT_MAX;
    int r_bound = INT_MIN;
    for (auto depth = 0; depth < new_depth; ++depth)
    {
        l_bound = std::min((*combined)[depth].l, l_bound);
        r_bound = std::max((*combined)[depth].r, r_bound);
    }

    combined->setBoundingBox({l_bound, r_bound});

    layout.setShape(nid, std::move(combined));
}

/// Calculate shape for sized rectangle (lantern); size is between 0 and 127
static ShapeUniqPtr calc_for_sized_rect(int size)
{

    // using namespace lantern;

    int levels = std::ceil((size * lantern::K + lantern::BASE_HEIGHT) / (float)layout::dist_y) + 1;

    auto shape = ShapeUniqPtr(new Shape(levels));

    for (auto i = 0u; i < levels; ++i)
    {
        (*shape)[i] = {-lantern::HALF_WIDTH, lantern::HALF_WIDTH};
    }

    shape->setBoundingBox({-lantern::HALF_WIDTH, lantern::HALF_WIDTH});

    return std::move(shape);
}

/// Computes layout for nid (shape, bounding box, offsets for its children)
void LayoutCursor::computeForNode(NodeID nid)
{
    const bool hidden = m_vis_flags.isHidden(nid);
    const bool label_shown = m_vis_flags.isLabelShown(nid);

    /// Check if the node is hidden:
    if (hidden)
    {
        const auto lsize = m_vis_flags.lanternSize(nid);

        if (lsize > -1)
        {
            /// Lantern node
            auto shape = calc_for_sized_rect(lsize);
            if (label_shown)
            {
                /// overriting the first extent in case of a label
                (*shape)[0] = calculateForSingleNode(nid, tree_, label_shown, true, debug_mode_);
                shape->setBoundingBox({(*shape)[0].l, (*shape)[0].r});
            }
            m_layout.setShape(nid, std::move(shape));
        }
        else
        {
            /// Normal failure node (triangle)

            if (!label_shown)
            {
                m_layout.setShape(nid, ShapeUniqPtr(&Shape::hidden));
            }
            else
            {
                auto shape = ShapeUniqPtr{new Shape{2}};
                (*shape)[0] = calculateForSingleNode(nid, tree_, label_shown, true, debug_mode_);
                (*shape)[1] = (*shape)[0];
                shape->setBoundingBox({(*shape)[0].l, (*shape)[0].r});
                m_layout.setShape(nid, std::move(shape));
            }
        }
    }
    else
    {
        auto nkids = tree_.childrenCount(nid);

        if (nkids == 0)
        {
            if (!m_vis_flags.isLabelShown(nid))
            {
                auto other = ShapeUniqPtr(&Shape::leaf);
                // m_layout.setShape(nid, ShapeUniqPtr(&Shape::leaf));
                m_layout.setShape(nid, std::move(other));
            }
            else
            {
                auto shape = ShapeUniqPtr{new Shape{1}};
                (*shape)[0] = calculateForSingleNode(nid, tree_, label_shown, false, debug_mode_);

                shape->setBoundingBox({(*shape)[0].l, (*shape)[0].r});
                m_layout.setShape(nid, std::move(shape));
            }
        }
        else if (nkids == 1)
        {

            const auto kid = tree_.getChild(nid, 0);
            const auto &kid_s = *m_layout.getShape(kid);

            auto shape = ShapeUniqPtr(new Shape(kid_s.height() + 1));

            (*shape)[0] = calculateForSingleNode(nid, tree_, label_shown, false, debug_mode_);

            for (auto depth = 0; depth < kid_s.height(); depth++)
            {
                (*shape)[depth + 1] = kid_s[depth];
            }

            shape->setBoundingBox(kid_s.boundingBox());

            m_layout.setChildOffset(kid, 0);

            m_layout.setShape(nid, std::move(shape));
        }
        else if (nkids == 2)
        {
            computeForNodeBinary(nid, m_layout, tree_, label_shown, debug_mode_);
        }
        else if (nkids > 2)
        {
            computeForNodeNary(nid, nkids, m_layout, tree_, debug_mode_);
        }
    }

    /// Layout is done for `nid` and its children
    m_layout.setLayoutDone(nid, true);
}

void LayoutCursor::processCurrentNode()
{

    const auto dirty = m_layout.isDirty(cur_node());
    // auto layout_done = m_layout.getLayoutDone(cur_node());

    if (dirty)
    {
        computeForNode(cur_node());
        m_layout.setDirty(cur_node(), false);
    }
}

void LayoutCursor::finalize()
{
    // std::cerr << "\n";
}

} // namespace tree
} // namespace cpprofiler