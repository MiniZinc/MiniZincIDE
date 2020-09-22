#include <QObject>
#include "../core.hh"

namespace cpprofiler
{
namespace tree
{

class NodeStats : public QObject
{
    Q_OBJECT
    int branch_ = 0;
    int undet_ = 0;
    int failed_ = 0;
    int skipped_ = 0;
    int solved_ = 0;
    int max_depth_ = 0;

  public:
    int branchCount() const { return branch_; }

    int undeterminedCount() const { return undet_; }

    int failedCount() const { return failed_; }

    int skippedCount() const { return skipped_; }

    int solvedCount() const { return solved_; }

    int maxDepth() const { return max_depth_; }

    void add_branch(int n)
    {
        branch_ = branch_ + n;
    }

    void subtract_undetermined(int n)
    {
        undet_ = undet_ - n;
    }

    void add_undetermined(int n)
    {
        undet_ = undet_ + n;
    }

    void add_failed(int n)
    {
        failed_ = failed_ + n;
    }

    void add_solved(int n)
    {
        solved_ = solved_ + n;
    }

    void add_skipped(int n)
    {
        skipped_ += n;
    }

    void addNode(NodeStatus status)
    {

        switch (status)
        {
        case NodeStatus::BRANCH:
        {
            add_branch(1);
        }
        break;
        case NodeStatus::FAILED:
        {
            add_failed(1);
        }
        break;
        case NodeStatus::SOLVED:
        {
            add_solved(1);
        }
        break;
        case NodeStatus::SKIPPED:
        {
            add_skipped(1);
        }
        break;
        case NodeStatus::UNDETERMINED:
        {
            undet_ += 1;
        }
        break;
        }
    }

    /// see if max depth needs to be updated to d
    void inform_depth(int d)
    {
        if (d > max_depth_)
        {
            max_depth_ = d;
        }
    }

  signals:

    void stats_changed();
};

} // namespace tree
} // namespace cpprofiler