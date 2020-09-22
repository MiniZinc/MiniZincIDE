#include "node_info.hh"
#include "node.hh"
#include <QDebug>

namespace cpprofiler
{
namespace tree
{

constexpr NumFlagLoc STATUS = {0, 4};

void NodeInfoEntry::setFlag(NodeFlag flag, bool value)
{
    m_bitset[static_cast<int>(flag)] = value;
}

bool NodeInfoEntry::getFlag(NodeFlag flag) const
{
    return m_bitset[static_cast<int>(flag)];
}

void NodeInfoEntry::setNumericFlag(NumFlagLoc loc, int value)
{
    uint32_t mask = (1 << loc.len) - 1;
    uint32_t clearmask = ~(mask << loc.pos);
    m_bitset &= std::bitset<4>(clearmask);
    m_bitset |= (value & mask) << loc.pos;
}

int NodeInfoEntry::getNumericFlag(NumFlagLoc loc) const
{
    uint32_t mask = (1 << loc.len) - 1;
    return ((m_bitset >> loc.pos) & std::bitset<4>(mask)).to_ulong();
}

} // namespace tree
} // namespace cpprofiler

namespace cpprofiler
{
namespace tree
{

NodeStatus NodeInfo::getStatus(NodeID nid) const
{
    utils::MutexLocker lock(&m_mutex, "node info");
    if (static_cast<int>(nid) >= m_flags.size())
        throw;
    return static_cast<NodeStatus>(m_flags[nid].getNumericFlag(STATUS));
}

void NodeInfo::setStatus(NodeID nid, NodeStatus status)
{
    utils::MutexLocker lock(&m_mutex, "node info");
    if (static_cast<int>(nid) >= m_flags.size())
        throw;
    m_flags[nid].setNumericFlag(STATUS, static_cast<int>(status));
}

void NodeInfo::addEntry(NodeID nid)
{
    utils::MutexLocker lock(&m_mutex, "node info");
    if (nid != m_flags.size())
        throw;
    m_flags.push_back({});
    m_has_solved_children.push_back(false);
    m_has_open_children.push_back(true);
}

void NodeInfo::setHasSolvedChildren(NodeID nid, bool val)
{
    m_has_solved_children[nid] = val;
}

bool NodeInfo::hasSolvedChildren(NodeID nid) const
{
    return m_has_solved_children[nid];
}

void NodeInfo::setHasOpenChildren(NodeID nid, bool val)
{
    m_has_open_children[nid] = val;
}

bool NodeInfo::hasOpenChildren(NodeID nid) const
{
    return m_has_open_children[nid];
}

} // namespace tree
} // namespace cpprofiler