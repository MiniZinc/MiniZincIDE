#include "layout.hh"

#include <QDebug>
#include <iostream>

#include "../utils/std_ext.hh"

namespace cpprofiler
{
namespace tree
{

void Layout::setShape(NodeID nid, std::unique_ptr<Shape, ShapeDeleter> shape)
{
    shapes_[nid] = std::move(shape);
}

utils::Mutex &Layout::getMutex() const
{
    return layout_;
}

Layout::Layout()
{
}

Layout::~Layout() = default;

int Layout::getHeight(NodeID nid) const
{
    return getShape(nid)->height();
}

bool Layout::ready(NodeID nid) const
{
    return (layout_done_.size() > nid);
}

bool Layout::getLayoutDone(NodeID nid) const
{
    if (layout_done_.size() <= nid)
        return false;
    return layout_done_[nid];
}

void Layout::growDataStructures(int n_nodes)
{

    if (n_nodes > shapes_.size())
    {
        child_offsets_.resize(n_nodes, 0);
        shapes_.resize(n_nodes);
        layout_done_.resize(n_nodes, false);
        /// nodes start as dirty
        dirty_.resize(n_nodes, true);
    }
}

} // namespace tree
} // namespace cpprofiler
