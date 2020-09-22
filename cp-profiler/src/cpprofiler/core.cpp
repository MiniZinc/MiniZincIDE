#include "core.hh"

namespace cpprofiler
{
namespace tree
{

std::ostream &operator<<(std::ostream &os, const NodeStatus &ns)
{
    switch (ns)
    {
    case NodeStatus::SOLVED:
    {
        os << "SOLVED";
    }
    break;
    case NodeStatus::FAILED:
    {
        os << "FAILED";
    }
    break;
    case NodeStatus::BRANCH:
    {
        os << "BRANCH";
    }
    break;
    case NodeStatus::SKIPPED:
    {
        os << "SKIPPED";
    }
    break;
    case NodeStatus::UNDETERMINED:
    {
        os << "UNDETERMINED";
    }
    break;
    case NodeStatus::MERGED:
    {
        os << "MERGED";
    }
    break;
    default:
    {
        os << "<unknown node status>";
    }
    break;
    }
    return os;
}

} // namespace tree
} // namespace cpprofiler

namespace cpprofiler
{
const Nogood Nogood::empty("");
}