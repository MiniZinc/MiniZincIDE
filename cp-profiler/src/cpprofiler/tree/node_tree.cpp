#include "node_tree.hh"

#include "structure.hh"
#include "node_info.hh"
#include "../solver_data.hh"
#include "../utils/tree_utils.hh"
#include "../name_map.hh"
#include <QDebug>
#include <cassert>

namespace cpprofiler
{
namespace tree
{

NodeTree::NodeTree() : structure_{new Structure()}, node_info_(new NodeInfo)
{
    qRegisterMetaType<NodeID>();
}

NodeTree::~NodeTree() = default;

void NodeTree::setSolverData(std::shared_ptr<SolverData> sd)
{
    solver_data_ = sd;
}

void NodeTree::setNameMap(std::shared_ptr<const NameMap> nm)
{
    name_map_ = nm;
}

void NodeTree::addEntry(NodeID nid)
{
    node_info_->addEntry(nid);
    labels_.push_back({});
}

const NodeInfo &NodeTree::node_info() const
{
    return *node_info_;
}

NodeInfo &NodeTree::node_info()
{
    return *node_info_;
}

void NodeTree::promoteNode(NodeID nid, int kids, tree::NodeStatus status, Label label)
{

    /// find parent and kid's position
    const auto pid = getParent(nid);
    const auto alt = getAlternative(nid);

    promoteNode(pid, alt, kids, status, label);
}

NodeID NodeTree::createRoot(int kids, Label label)
{
    auto nid = structure_->createRoot(kids);
    addEntry(nid);
    setLabel(nid, label);

    auto depth = kids > 0 ? 2 : 1;

    node_stats_.inform_depth(depth);
    node_stats_.add_branch(1);
    node_info_->setStatus(nid, NodeStatus::BRANCH);

    for (auto i = 0; i < kids; ++i)
    {
        auto child_nid = structure_->getChild(nid, i);
        addEntry(child_nid);
        node_info_->setStatus(child_nid, NodeStatus::UNDETERMINED);
    }

    node_stats_.add_undetermined(kids);

    emit structureUpdated();

    return nid;
}

/// Note that this form does not create children
void NodeTree::db_createRoot(NodeID nid, Label label)
{

    structure_->db_createRoot(nid);
    addEntry(nid);
    setLabel(nid, label);

    node_stats_.inform_depth(1);
    node_stats_.add_branch(1);
    node_info_->setStatus(nid, NodeStatus::BRANCH);
}

static bool is_closing(NodeStatus status)
{
    return (status == NodeStatus::FAILED) ||
           (status == NodeStatus::SOLVED) ||
           (status == NodeStatus::SKIPPED);
}

/// Note: alt is unnecessary here
void NodeTree::db_addChild(NodeID nid, NodeID pid, int alt, NodeStatus status, Label label)
{
    structure_->db_addChild(nid, pid, alt);
    addEntry(nid);

    node_info_->setStatus(nid, status);
    setLabel(nid, label);

    emit childrenStructureChanged(pid);

    auto cur_depth = utils::calculate_depth(*this, nid);
    node_stats_.inform_depth(cur_depth);

    node_stats_.addNode(status);

    if (is_closing(status))
        closeNode(nid);
    if (status == NodeStatus::SOLVED)
        notifyAncestors(nid);

    emit structureUpdated();
}

void NodeTree::addExtraChild(NodeID pid)
{
    const auto nid = structure_->addExtraChild(pid);
    addEntry(nid);

    node_info_->setStatus(nid, NodeStatus::UNDETERMINED);
    node_stats_.add_undetermined(1);

    emit childrenStructureChanged(pid);

    emit structureUpdated();
}

NodeID NodeTree::promoteNode(NodeID parent_id, int alt, int kids, tree::NodeStatus status, Label label)
{

    NodeID nid;

    if (parent_id == NodeID::NoNode)
    {
        /// This makes it possible to transform an undet root node into a branch node,
        /// necessary, for example, in merging
        nid = structure_->getRoot();
    }
    else
    {
        nid = structure_->getChild(parent_id, alt);
    }

    node_info_->setStatus(nid, status);
    setLabel(nid, label);
    // setLabel(nid, std::to_string(nid));

    if (kids > 0)
    {
        structure_->addChildren(nid, kids);
        emit childrenStructureChanged(nid); /// updates dirty status for nodes

        for (auto i = 0; i < kids; ++i)
        {
            auto child_nid = structure_->getChild(nid, i);
            addEntry(child_nid);
            node_info_->setStatus(child_nid, NodeStatus::UNDETERMINED);
        }

        node_stats_.add_undetermined(kids);

        auto cur_depth = utils::calculate_depth(*this, nid);
        node_stats_.inform_depth(cur_depth + 1);
    }

    node_stats_.subtract_undetermined(1);
    node_stats_.addNode(status);

    if (status == NodeStatus::SOLVED)
        notifyAncestors(nid);

    if (is_closing(status))
        closeNode(nid);

    if (childrenCount(nid) != kids)
    {
        print("error: replacing existing node");
    }
    // assert( childrenCount(nid) == kids );

    emit structureUpdated();

    return nid;
}

int NodeTree::nodeCount() const
{
    return structure_->nodeCount();
}

NodeID NodeTree::getParent(NodeID nid) const
{
    return structure_->getParent(nid);
}

NodeID NodeTree::getRoot() const
{
    return structure_->getRoot();
}

NodeID NodeTree::getChild(NodeID nid, int alt) const
{
    return structure_->getChild(nid, alt);
}

const NodeStats &NodeTree::node_stats() const
{
    return node_stats_;
}

const SolverData &NodeTree::solver_data() const
{
    return *solver_data_;
}

utils::Mutex &NodeTree::treeMutex() const
{
    return structure_->getMutex();
}

NodeStatus NodeTree::getStatus(NodeID nid) const
{
    return node_info_->getStatus(nid);
}

int NodeTree::getNumberOfSiblings(NodeID nid) const
{
    return structure_->getNumberOfSiblings(nid);
}

int NodeTree::depth() const
{
    return node_stats_.maxDepth();
}

int NodeTree::getAlternative(NodeID nid) const
{
    return structure_->getAlternative(nid);
}

int NodeTree::childrenCount(NodeID nid) const
{
    return structure_->childrenCount(nid);
}

void NodeTree::notifyAncestors(NodeID nid)
{
    while (nid != NodeID::NoNode)
    {
        node_info_->setHasSolvedChildren(nid, true);
        nid = getParent(nid);
    }
}

void NodeTree::setHasOpenChildren(NodeID nid, bool val)
{
    node_info_->setHasOpenChildren(nid, val);
}

const Label NodeTree::getLabel(NodeID nid) const
{
    // return std::to_string(nid);

    // auto uid = solver_data_->getSolverID(nid);
    // return uid.toString();

    auto &orig = labels_.at(nid);
    if (name_map_)
    {
        return name_map_->replaceNames(orig);
    }
    return orig;
}

const Nogood &NodeTree::getNogood(NodeID nid) const
{
    return solver_data_->getNogood(nid);
}

bool NodeTree::hasSolvedChildren(NodeID nid) const
{
    return node_info_->hasSolvedChildren(nid);
}

bool NodeTree::hasOpenChildren(NodeID nid) const
{
    return node_info_->hasOpenChildren(nid);
}

bool NodeTree::isOpen(NodeID nid) const
{
    return ((getStatus(nid) == NodeStatus::UNDETERMINED) ||
            hasOpenChildren(nid));
}

void NodeTree::onChildClosed(NodeID nid)
{

    bool allClosed = true;

    for (auto i = childrenCount(nid); i--;)
    {
        auto kid = getChild(nid, i);
        if (isOpen(kid))
        {
            allClosed = false;
            break;
        }
    }

    if (allClosed)
    {
        closeNode(nid);

        if (!hasSolvedChildren(nid))
        {
            emit failedSubtreeClosed(nid);
        }
    }
}

void NodeTree::closeNode(NodeID nid)
{
    setHasOpenChildren(nid, false);
    auto pid = getParent(nid);
    if (pid != NodeID::NoNode)
    {
        onChildClosed(pid);
    }
}

void NodeTree::setLabel(NodeID nid, const Label &label)
{
    labels_[nid] = label;
}

void NodeTree::removeNode(NodeID nid)
{

    const auto pid = getParent(nid);
    if (pid == NodeID::NoNode)
        return;

    const auto alt = getAlternative(nid);
    /// should this really remove the node?
    structure_->removeChild(pid, alt);
}

void NodeTree::db_initialize(int size)
{
    structure_->db_initialize(size);
}

} // namespace tree
} // namespace cpprofiler