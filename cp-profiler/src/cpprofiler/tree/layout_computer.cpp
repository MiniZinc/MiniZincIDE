#include "layout_computer.hh"
#include "layout.hh"
#include "structure.hh"
#include "node_tree.hh"
#include "shape.hh"
#include "../utils/std_ext.hh"
#include "../utils/perf_helper.hh"

#include "cursors/layout_cursor.hh"
#include "cursors/nodevisitor.hh"

#include <QMutex>
#include <QDebug>
#include <thread>
#include <iostream>

namespace cpprofiler
{
namespace tree
{

LayoutComputer::LayoutComputer(const NodeTree &tree, Layout &layout, const VisualFlags &nf)
    : m_tree(tree), m_layout(layout), m_vis_flags(nf)
{
}

bool LayoutComputer::isDirty(NodeID nid)
{
    return m_layout.isDirty(nid);
}

void LayoutComputer::setDirty(NodeID nid)
{

    utils::MutexLocker lock(&m_layout.getMutex());

    m_layout.setDirty(nid, true);
}

void LayoutComputer::dirtyUpUnconditional(NodeID n)
{
    while (n != NodeID::NoNode)
    {
        m_layout.setDirty(n, true);
        n = m_tree.getParent(n);
    }
}

void LayoutComputer::dirtyUp(NodeID nid)
{
    while (nid != NodeID::NoNode && !m_layout.isDirty(nid))
    {
        m_layout.setDirty(nid, true);
        nid = m_tree.getParent(nid);
    }
}

void LayoutComputer::dirtyUpLater(NodeID nid)
{
    // if (m_layout.ready(nid))
    // {
    // print("dirty up {} later", nid);
    du_node_set_.insert(nid);
    // }
}

bool LayoutComputer::compute()
{

    /// do nothing if there is no nodes

    if (m_tree.nodeCount() == 0)
        return false;

    /// TODO: come back here (ensure mutexes work correctly)
    utils::MutexLocker tree_lock(&m_tree.treeMutex());
    utils::MutexLocker layout_lock(&m_layout.getMutex());

    /// Ensures that sufficient memory is allocated for every node's shape
    m_layout.growDataStructures(m_tree.nodeCount());

    // print("to dirty up size: {}", du_node_set_.size());

    for (auto n : du_node_set_)
    {
        dirtyUp(n);
    }

    du_node_set_.clear();

    LayoutCursor lc(m_tree.getRoot(), m_tree, m_vis_flags, m_layout, debug_mode_);
    PostorderNodeVisitor<LayoutCursor>(lc).run();

    static int counter = 0;
    // std::cerr << "computed layout " << ++counter   << " times\n";

    return true;
}

} // namespace tree
} // namespace cpprofiler