#include "structure.hh"

#include <iostream>
#include <exception>
#include <stack>
#include <algorithm>
#include <QDebug>

using cpprofiler::utils::Mutex;

namespace cpprofiler
{
namespace tree
{

Structure::Structure()
{
    nodes_.reserve(100);
}

Mutex &Structure::getMutex() const
{
    return mutex_;
}

NodeID Structure::createRoot(int kids)
{
    if (nodes_.size() > 0)
    {
        throw invalid_tree();
    }

    const auto root_nid = createNode(NodeID::NoNode, kids);

    /// create white nodes for children nodes
    for (auto i = 0; i < kids; ++i)
    {
        createChild(root_nid, i, 0);
    }

    return root_nid;
}

NodeID Structure::createNode(NodeID pid, int kids)
{
    const auto nid = NodeID{static_cast<int>(nodes_.size())};
    nodes_.push_back(std::unique_ptr<Node>{new Node(pid, kids)});
    return nid;
}

void Structure::db_createNode(NodeID nid, NodeID pid, int kids)
{
    nodes_[nid] = std::unique_ptr<Node>{new Node(pid, kids)};
}

NodeID Structure::createChild(NodeID pid, int alt, int kids)
{
    const auto nid = createNode(pid, kids);
    nodes_[pid]->setChild(nid, alt);
    return nid;
}

NodeID Structure::addExtraChild(NodeID pid)
{

    const auto alt = childrenCount(pid);

    /// make room for another child
    nodes_[pid]->addChild();

    auto kid = createChild(pid, alt, 0);
    return kid;
}

void Structure::addChildren(NodeID nid, int kids)
{
    if (nodes_[nid]->childrenCount() > 0)
        throw;

    nodes_[nid]->setNumberOfChildren(kids);

    for (auto i = 0; i < kids; ++i)
    {
        createChild(nid, i, 0);
    }
}

/// Remove `alt` child of `pid`
void Structure::removeChild(NodeID pid, int alt)
{
    nodes_[pid]->removeChild(alt);
}

NodeID Structure::getChild(NodeID pid, int alt) const
{
    return nodes_[pid]->getChild(alt);
}

NodeID Structure::getParent(NodeID nid) const
{
    return nodes_[nid]->getParent();
}

int Structure::childrenCount(NodeID pid) const
{
    return nodes_[pid]->childrenCount();
}

int Structure::getNumberOfSiblings(NodeID nid) const
{
    auto pid = getParent(nid);
    return childrenCount(pid);
}

NodeID Structure::getRoot() const
{
    return NodeID{0};
}

int Structure::getAlternative(NodeID nid) const
{
    auto parent_nid = getParent(nid);

    if (parent_nid == NodeID::NoNode)
        return -1;

    for (auto i = 0; i < childrenCount(parent_nid); ++i)
    {
        if (getChild(parent_nid, i) == nid)
        {
            return i;
        }
    }
    throw;
    return -1;
}

int Structure::nodeCount() const
{
    return nodes_.size();
}

void Structure::db_initialize(int size)
{
    nodes_.resize(size);
}

void Structure::db_createRoot(NodeID nid)
{
    db_createNode(nid, NodeID::NoNode, 0);
}

void Structure::db_createChild(NodeID nid, NodeID pid, int alt)
{
    db_createNode(nid, pid, 0);
    nodes_[pid]->setChild(nid, alt);
}

void Structure::db_addChild(NodeID nid, NodeID pid, int alt)
{
    nodes_[pid]->addChild();
    db_createChild(nid, pid, alt);
}

} // namespace tree
} // namespace cpprofiler