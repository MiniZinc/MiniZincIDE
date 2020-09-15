#ifndef CPPROFILER_GLOBAL_HH
#define CPPROFILER_GLOBAL_HH

#include <ostream>
#include "tree/node_id.hh"
#include "utils/debug_mutex.hh"
#include "utils/std_ext.hh"
#include "utils/debug.hh"

using cpprofiler::tree::NodeID;
using Label = std::string;
using ExecID = int;

using NogoodID = cpprofiler::tree::NodeID;

namespace cpprofiler
{
namespace tree
{

enum class NodeStatus
{
    SOLVED = 0,
    FAILED = 1,
    BRANCH = 2,
    SKIPPED = 3,
    UNDETERMINED = 4,
    MERGED = 5
};

std::ostream &operator<<(std::ostream &os, const NodeStatus &ns);

} // namespace tree
} // namespace cpprofiler

namespace cpprofiler
{

typedef std::string Info;

class Nogood
{
    /// whether the nogood has a renamed (nice) version
    bool renamed_;
    /// raw flatzinc nogood
    std::string orig_ng_;
    /// built using a name map
    std::string nice_ng_;

  public:
    explicit Nogood(const std::string &orig) : renamed_(false), orig_ng_(orig) {}

    Nogood(const std::string &orig, const std::string &renamed) : renamed_(true), orig_ng_(orig), nice_ng_(renamed) {}

    const std::string &renamed() const
    {
        return nice_ng_;
    }

    bool has_renamed() const { return renamed_; }

    /// Get the best name available (renamed if present)
    const std::string &get() const { return renamed_ ? nice_ng_ : orig_ng_; }

    const std::string &original() const
    {
        return orig_ng_;
    }

    static const Nogood empty;
};
} // namespace cpprofiler

#endif