#ifndef CPPROFILER_TREE_NODE_ID_HH
#define CPPROFILER_TREE_NODE_ID_HH

#include <cstdint>
#include <functional>

#include <QMetaType>

namespace cpprofiler
{
namespace tree
{

class NodeID
{
    /// Internal representation
    int id;

  public:
    /// Used to indicate invalid or non-existing nodes
    static NodeID NoNode;

    /// Allow implicit conversion to int
    operator int() const { return id; }
    /// Allow explicit conversion from int
    explicit NodeID(int nid = -1) : id(nid) {}
};

} // namespace tree
} // namespace cpprofiler

/// Allow signal-slot mechanism across threads to use NodeID
Q_DECLARE_METATYPE(cpprofiler::tree::NodeID)

/// Make node hashable (to be used as a key)
namespace std
{
template <>
struct hash<cpprofiler::tree::NodeID>
{
    size_t operator()(const cpprofiler::tree::NodeID &nid) const
    {
        return std::hash<int>{}(static_cast<int>(nid));
    }
};
} // namespace std

/// Allow comparison between NodeIDs
namespace cpprofiler
{
namespace tree
{
bool operator==(const NodeID &lhs, const NodeID &rhs);
}
} // namespace cpprofiler

#endif