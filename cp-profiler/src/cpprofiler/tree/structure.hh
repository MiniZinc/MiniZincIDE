#ifndef CPPROFILER_NODE_TREE_HH
#define CPPROFILER_NODE_TREE_HH

#include "node.hh"

#include "../core.hh"

#include "memory"

namespace cpprofiler
{
namespace tree
{

struct invalid_tree : std::exception
{
    const char *what() const throw() override
    {
        return "invalid tree\n";
    }
};

class LayoutComputer;

/// Structure is responsible for dealing with the tree's structural information.
/// Since it is not aware of statuses, labels etc., it is the caller's responsibility
/// to ensure that this information is stored elsewhere when, for example, new nodes
/// are created using Structure's API.
class Structure
{

    /// Protects `nodes_`
    mutable utils::Mutex mutex_;

    /// Nodes that describe the structure of the tree
    std::vector<std::unique_ptr<Node>> nodes_;

    /// Allocate memory for a new node and return its Id
    NodeID createNode(NodeID pid, int kids);

    /// Create a new node with `kids` kids and add set it as the `alt`th kid of `pid`
    NodeID createChild(NodeID pid, int alt, int kids);

    void db_createNode(NodeID nid, NodeID pid, int kids);

  public:
    Structure();

    /// Get mutex protecting structural information
    utils::Mutex &getMutex() const;

    /// Get the identifier of the root (should be 0)
    NodeID getRoot() const;

    /// Get the parent of node `nid` (returns NodeID::NoNode for root)
    NodeID getParent(NodeID nid) const;

    /// Get the child of node `pid` at position `alt`
    NodeID getChild(NodeID pid, int alt) const;

    /// Get the position of node `nid` relative to its left-most sibling
    int getAlternative(NodeID nid) const;

    /// Get the total nuber of siblings of `nid` including the node itself
    int getNumberOfSiblings(NodeID nid) const;

    /// Get the total number of children of node `pid`
    int childrenCount(NodeID pid) const;

    /// Get the total nuber of nodes (including undetermined)
    int nodeCount() const;

    /// ************ Modifying (building) a tree ************

    /// Create a root node and `kids` children
    NodeID createRoot(int kids);

    /// Extend node's children with one more child
    NodeID addExtraChild(NodeID nid);

    /// Add `kids` children to an open node
    void addChildren(NodeID nid, int kids);

    /// Remove `alt` child of `pid`
    void removeChild(NodeID pid, int alt);

    /// ************ Building a tree from a database ************

    void db_initialize(int size);

    void db_createRoot(NodeID nid);

    void db_createChild(NodeID nid, NodeID pid, int alt);

    void db_addChild(NodeID nid, NodeID pid, int alt);
};

} // namespace tree
} // namespace cpprofiler

#endif