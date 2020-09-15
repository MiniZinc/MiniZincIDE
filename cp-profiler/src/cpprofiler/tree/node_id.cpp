#include "node_id.hh"

namespace cpprofiler
{
namespace tree
{

bool operator==(const NodeID &lhs, const NodeID &rhs)
{
    return (static_cast<int>(lhs) == static_cast<int>(rhs));
}

NodeID NodeID::NoNode = NodeID{-1};

} // namespace tree
} // namespace cpprofiler