#ifndef CPPROFILER_TREE_NODE_INFO
#define CPPROFILER_TREE_NODE_INFO

#include "../core.hh"

#include <bitset>
#include <vector>
#include <QMutex>
#include "node_id.hh"

namespace cpprofiler
{
namespace tree
{

enum class NodeStatus;

enum class NodeFlag
{

};

struct NumFlagLoc
{
    int pos;
    int len;
};

class NodeInfoEntry
{

  private:
    std::bitset<4> m_bitset;

  public:
    void setFlag(NodeFlag flag, bool value);
    bool getFlag(NodeFlag flag) const;

    void setNumericFlag(NumFlagLoc loc, int value);
    int getNumericFlag(NumFlagLoc loc) const;
};

class NodeInfo
{

    mutable utils::Mutex m_mutex;

    std::vector<NodeInfoEntry> m_flags;

    std::vector<bool> m_has_solved_children;
    std::vector<bool> m_has_open_children;

  public:
    NodeStatus getStatus(NodeID nid) const;
    void setStatus(NodeID nid, NodeStatus status);

    void addEntry(NodeID nid);

    void setHasSolvedChildren(NodeID nid, bool val);
    bool hasSolvedChildren(NodeID nid) const;

    void setHasOpenChildren(NodeID nid, bool val);
    bool hasOpenChildren(NodeID nid) const;
};

} // namespace tree
} // namespace cpprofiler

#endif