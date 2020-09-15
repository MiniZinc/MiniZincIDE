#pragma once

#include <QMainWindow>
#include "../tree/node_tree.hh"
#include "merging/merge_result.hh"

namespace cpprofiler
{
namespace tree
{
class TraditionalView;
}

class Execution;
class UserData;
} // namespace cpprofiler

namespace cpprofiler
{
namespace analysis
{

class PentagonCounter;
class PentagonListWidget;

/// Original location for a node;
struct OriginalLoc
{
    NodeID nid;
};

class MergeWindow : public QMainWindow
{
    Q_OBJECT

    /// The two executions merged
    Execution &ex_l_;
    Execution &ex_r_;

    std::shared_ptr<tree::NodeTree> nt_;
    std::shared_ptr<MergeResult> merge_result_;

    /// Dummy user data (required for traditional view)
    std::unique_ptr<UserData> user_data_;

    /// Dummy solver data (required for traditional view)
    std::unique_ptr<SolverData> solver_data_;

    std::unique_ptr<tree::TraditionalView> view_;

    PentagonCounter *pentagon_bar;

    PentagonListWidget *pent_list;

    /// Original locations for node `i` (used for nogoods/labels etc)
    std::vector<OriginalLoc> orig_locations_;

  private:
    /// find the right data for a node
    Nogood getNogood();

    /// traverse the merged tree to find origins for all nodes
    void initOrigLocations();
    /// Find id for a node `nid` of a merged tree
    // NodeID findOriginalId(NodeID nid) const;

    /// Hide all failed subtrees under pentagons
    void hideFailed();

  public:
    MergeWindow(Execution &ex_l, Execution &ex_r, std::shared_ptr<tree::NodeTree> nt, std::shared_ptr<MergeResult> res, QWidget* parent = nullptr);
    ~MergeWindow();

    tree::NodeTree &getTree();

    MergeResult &mergeResult();

  public slots:

    void runNogoodAnalysis() const;
};

} // namespace analysis
} // namespace cpprofiler