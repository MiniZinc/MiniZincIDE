#include "visual_flags.hh"

namespace cpprofiler
{
namespace tree
{

void VisualFlags::ensure_id_exists(NodeID nid)
{
    const auto id = static_cast<int>(nid);
    if (label_shown_.size() < id + 1)
    {
        label_shown_.resize(id + 1);
        node_hidden_.resize(id + 1);
        shape_highlighted_.resize(id + 1);
    }
}

void VisualFlags::setLabelShown(NodeID nid, bool val)
{
    ensure_id_exists(nid);
    label_shown_[nid] = val;
}

bool VisualFlags::isLabelShown(NodeID nid) const
{

    if (label_shown_.size() <= nid)
        return false;
    return label_shown_.at(nid);
}

void VisualFlags::setHidden(NodeID nid, bool val)
{
    ensure_id_exists(nid);
    node_hidden_[nid] = val;

    if (val)
    {
        hidden_nodes_.insert(nid);
    }
    else
    {
        hidden_nodes_.erase(nid);
    }
}

bool VisualFlags::isHidden(NodeID nid) const
{
    if (node_hidden_.size() <= nid)
        return false;
    return node_hidden_.at(nid);
}

void VisualFlags::unhideAll()
{

    for (auto &&n : node_hidden_)
    {
        n = false;
    }

    hidden_nodes_.clear();
}

int VisualFlags::hiddenCount()
{
    return hidden_nodes_.size();
}

void VisualFlags::setHighlighted(NodeID nid, bool val)
{
    ensure_id_exists(nid);

    if (val)
    {
        highlighted_shapes_.insert(nid);
    }
    else
    {
        highlighted_shapes_.erase(nid);
    }
    shape_highlighted_[nid] = val;
}

bool VisualFlags::isHighlighted(NodeID nid) const
{

    if (shape_highlighted_.size() <= nid)
    {
        return false;
    }
    return shape_highlighted_[nid];
}

void VisualFlags::unhighlightAll()
{
    for (auto nid : highlighted_shapes_)
    {
        shape_highlighted_[nid] = false;
    }

    highlighted_shapes_.clear();
}

void VisualFlags::resetLanternSizes()
{
    lantern_sizes_.clear();
}

void VisualFlags::setLanternSize(NodeID nid, int val)
{
    lantern_sizes_.insert({nid, val});
}

int VisualFlags::lanternSize(NodeID nid) const
{
    const auto it = lantern_sizes_.find(nid);
    if (it != lantern_sizes_.end())
    {
        return it->second;
    }
    else
    {
        return -1; /// for non-lantern nodes
    }
}

} // namespace tree
} // namespace cpprofiler