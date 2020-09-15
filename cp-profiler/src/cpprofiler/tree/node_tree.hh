#ifndef CPPROFILER_TREE_NODE_TREE_HH
#define CPPROFILER_TREE_NODE_TREE_HH

#include <QObject>
#include <memory>
#include <string>
#include <stack>
#include "node_id.hh"
#include "node.hh"
#include "../core.hh"

#include "node_stats.hh"

namespace cpprofiler
{
class NameMap;
class SolverData;
} // namespace cpprofiler

namespace cpprofiler
{
namespace tree
{

class Structure;
class NodeInfo;

static Label emptyLabel = {};

/// Node tree encapsulates tree structure, node statistics (number of nodes etc.),
/// status for nodes (node_info_), labels
class NodeTree : public QObject
{
    Q_OBJECT
    /// Tree structural information
    std::unique_ptr<Structure> structure_;
    /// Nodes' statuses and flags (has solved/open children etc.)
    std::unique_ptr<NodeInfo> node_info_;
    /// Mapping from ugly to nice names (if present, owned by Execution)
    std::shared_ptr<const NameMap> name_map_;
    /// Contains a mapping from node ids to their original solver ids (triplets)
    std::shared_ptr<SolverData> solver_data_;
    /// Nodes' labels
    std::vector<Label> labels_;
    /// Count of different types of nodes, tree depth
    NodeStats node_stats_;

    /// Indicates whether the tree is fully built
    bool is_done_ = false;

    /// Ensure all relevant data structures contain this node
    void addEntry(NodeID nid);

    /// Notify ancestor nodes of a solution
    void notifyAncestors(NodeID nid);

    /// Notify ancestor nodes that of whether they contain open nodes
    void onChildClosed(NodeID nid);

    /// Set closed and notify ancestors
    void closeNode(NodeID nid);

  public:
    NodeTree();
    ~NodeTree();

    const NodeInfo &node_info() const;
    NodeInfo &node_info();

    const NodeStats &node_stats() const;

    const SolverData &solver_data() const;

    cpprofiler::utils::Mutex &treeMutex() const;

    void setSolverData(std::shared_ptr<SolverData> sd);

    void setNameMap(std::shared_ptr<const NameMap> nm);

    void setDone() { is_done_ = true; }

    bool isDone() const { return is_done_; }

    /// *************************** Tree Modifiers ***************************

    NodeID createRoot(int kids, Label label = emptyLabel);

    /// turn a white node into some other node
    void promoteNode(NodeID nid, int kids, NodeStatus status, Label = emptyLabel);

    /// TODO: rename it to resetNode
    /// Turn undet node into a real one, updating stats and emitting signals
    NodeID promoteNode(NodeID parent_id, int alt, int kids, NodeStatus status, Label = emptyLabel);

    void addExtraChild(NodeID pid);

    /// Set the flag for open children
    void setHasOpenChildren(NodeID nid, bool val);

    void setLabel(NodeID nid, const Label &label);

    /// Remove node `nid` from the tree
    void removeNode(NodeID nid);

    /// ********************************************************************
    /// *************************** Tree Queries ***************************

    /// return the depth of the tree
    int depth() const;

    NodeID getRoot() const;

    /// Get the total nuber of nodes (including undetermined)
    int nodeCount() const;

    /// Get the total nuber of siblings of `nid` including the node itself
    int getNumberOfSiblings(NodeID nid) const;

    /// Get the position of node `nid` relative to its left-most sibling
    int getAlternative(NodeID nid) const;

    /// Get the total number of children of node `pid`
    int childrenCount(NodeID nid) const;

    /// Get the child of node `pid` at position `alt`
    NodeID getChild(NodeID nid, int alt) const;

    /// Get the parent of node `nid` (returns NodeID::NoNode for root)
    NodeID getParent(NodeID nid) const;

    /// Get the status of node `nid`
    NodeStatus getStatus(NodeID nid) const;

    /// Get the label of node `nid`
    const Label getLabel(NodeID nid) const;

    /// Get the nogood of node `nid`
    const Nogood &getNogood(NodeID nid) const;

    /// Check if the node `nid` has solved children (ancestors?)
    bool hasSolvedChildren(NodeID nid) const;

    /// Check if the node `nid` has open children (ancestors?)
    bool hasOpenChildren(NodeID nid) const;

    /// Check if the node `nid` is open or has open children
    bool isOpen(NodeID nid) const;

    /// ************ Building a tree from a database ************

    void db_initialize(int size);

    void db_createRoot(NodeID nid, Label label = emptyLabel);

    void db_addChild(NodeID nid, NodeID pid, int alt, NodeStatus status, Label = emptyLabel);

    /// ********************************************************************

  signals:

    /// Notifies that the strucutre of the tree changed in general
    /// and triggeres layout update
    void structureUpdated();

    /// Notifies that the structure underneath the node has changed
    /// and requires layout update
    void childrenStructureChanged(NodeID nid);

    /// Notify that all nodes in the subtree have been closed (no undetermined nodes)
    void failedSubtreeClosed(cpprofiler::tree::NodeID nid);
};

} // namespace tree
} // namespace cpprofiler

#endif