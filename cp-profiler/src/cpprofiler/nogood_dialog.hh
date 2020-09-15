#pragma once

#include <QDialog>
#include <QStandardItemModel>
#include "core.hh"

namespace cpprofiler
{

namespace tree
{
class NodeTree;
}

class NogoodDialog : public QDialog
{
    Q_OBJECT

    std::unique_ptr<QStandardItemModel> ng_model_;

  public:
    NogoodDialog(const tree::NodeTree &nt, const std::vector<NodeID> &nodes);

  signals:
    void nogoodClicked(NodeID nid);
};

} // namespace cpprofiler