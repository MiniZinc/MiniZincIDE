#pragma once

#include <QThread>
#include <memory>

#include "merging/merge_result.hh"

namespace cpprofiler
{

namespace tree
{
class NodeTree;
}

class Execution;
} // namespace cpprofiler

namespace cpprofiler
{
namespace analysis
{

struct OriginalLoc;

class TreeMerger : public QThread
{

  const Execution &ex_l;
  const Execution &ex_r;

  const tree::NodeTree &tree_l;
  const tree::NodeTree &tree_r;

  std::shared_ptr<tree::NodeTree> res_tree;
  std::shared_ptr<MergeResult> merge_result;

  std::shared_ptr<std::vector<OriginalLoc>> orig_locs_;

protected:
  void
  run() override;

public:
  TreeMerger(const Execution &ex_l,
             const Execution &ex_r,
             std::shared_ptr<tree::NodeTree> tree,
             std::shared_ptr<analysis::MergeResult> res,
             std::shared_ptr<std::vector<OriginalLoc>> orig_locs);
  ~TreeMerger();
};

} // namespace analysis
} // namespace cpprofiler