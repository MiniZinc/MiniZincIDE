#include "tree_builder.hh"

#include "message_wrapper.hh"

#include "utils/perf_helper.hh"
#include "utils/debug.hh"
#include "execution.hh"

#include "tree/node_tree.hh"
#include "name_map.hh"

#include <thread>

namespace cpprofiler
{

static std::ostream &operator<<(std::ostream &os, const NodeUID &uid)
{
    return os << "{" << uid.nid << ", " << uid.rid << ", " << uid.tid << "}";
}

static std::ostream &operator<<(std::ostream &os, const NodeStatus &status)
{
    switch (status)
    {
    case SOLVED:
        os << "SOLVED";
        break;
    case FAILED:
        os << "FAILED";
        break;
    case BRANCH:
        os << "BRANCH";
        break;
    case SKIPPED:
        os << "SKIPPED";
        break;
    }
    return os;
}

/// works correctly for node messages only atm
static std::ostream &operator<<(std::ostream &os, const Message &msg)
{
    os << "nid: " << msg.nodeUID() << ", pid: " << msg.parentUID();
    os << ", alt: " << msg.alt() << ", kids: " << msg.kids();
    os << ", " << msg.status();
    // if (msg.has_label()) os << ", label: " << msg.label();
    // if (msg.has_nogood()) os << ", nogood: " << msg.nogood();
    // if (msg.has_info()) os << ", info: " << msg.info();
    return os;
}

TreeBuilder::TreeBuilder(Execution &ex) : m_execution(ex)
{
    std::cerr << "  TreeBuilder()\n";
    startBuilding();
}

void TreeBuilder::startBuilding()
{
    perfHelper.begin("tree building");
    print("Builder: start building");
}

void TreeBuilder::finishBuilding()
{
    perfHelper.end();
    print("Builder: done building");
    emit buildingDone();
}

void TreeBuilder::handleNode(MessageWrapper& node)
{
    // print("node: {}", *node);
    auto& msg = node.msg();

    const auto n_uid = msg.nodeUID();
    const auto p_uid = msg.parentUID();

    auto &tree = m_execution.tree();

    tree::NodeID pid = tree::NodeID::NoNode;

    if (p_uid.nid != -1)
    {
        /// should solver data be moved to node tree?
        pid = m_execution.solver_data().getNodeId({p_uid.nid, p_uid.rid, p_uid.tid});
    }

    const auto kids = msg.kids();
    const auto alt = msg.alt();
    const auto status = static_cast<tree::NodeStatus>(msg.status());
    const auto &label = msg.has_label() ? msg.label() : tree::emptyLabel;

    NodeID nid;

    {
        utils::MutexLocker tree_lock(&tree.treeMutex(), "builder");

        if (pid == NodeID::NoNode)
        {

            if (m_execution.doesRestarts())
            {
                tree.addExtraChild(NodeID{0});
                nid = tree.promoteNode(NodeID{0}, restart_count++, kids, status, label);
            }
            else
            {
                nid = tree.createRoot(kids);
            }
        }
        else
        {
            nid = tree.promoteNode(pid, alt, kids, status, label);
        }
    }

    m_execution.solver_data().setNodeId({n_uid.nid, n_uid.rid, n_uid.tid}, nid);

    if (msg.has_nogood())
    {
        const auto nm = m_execution.nameMap();

        if (nm)
        {
            /// Construct a renamed nogood using the name map
            const auto renamed = m_execution.nameMap()->replaceNames(msg.nogood());
            m_execution.solver_data().setNogood(nid, msg.nogood(), renamed);
        }
        else
        {
            m_execution.solver_data().setNogood(nid, msg.nogood());
        }
    }

    if (msg.has_info() && !msg.info().empty())
    {
        m_execution.solver_data().processInfo(nid, msg.info());
    }
}

} // namespace cpprofiler
