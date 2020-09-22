#include "nogood_dialog.hh"
#include "tree/node_tree.hh"
#include "solver_data.hh"

#include <QTableView>
#include <QVBoxLayout>

#include <QHeaderView>

namespace cpprofiler
{

NogoodDialog::NogoodDialog(const tree::NodeTree &nt, const std::vector<NodeID> &nodes)
{

    static constexpr int DEFAULT_WIDTH = 800;
    static constexpr int DEFAULT_HEIGHT = 400;

    resize(DEFAULT_WIDTH, DEFAULT_HEIGHT);

    auto ng_table = new QTableView();

    auto lo = new QVBoxLayout(this);

    ng_model_.reset(new QStandardItemModel(0, 3));

    const QStringList headers{"NodeID", "SolverID", "Clause"};
    ng_model_->setHorizontalHeaderLabels(headers);
    ng_table->horizontalHeader()->setStretchLastSection(true);
    ng_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    ng_table->setEditTriggers(QAbstractItemView::NoEditTriggers);

    ng_table->setModel(ng_model_.get());

    const auto &sd = nt.solver_data();

    for (auto n : nodes)
    {

        const auto nid_item = new QStandardItem(QString::number(n));

        const auto sid = sd.getSolverID(n);
        const auto sid_item = new QStandardItem(sid.toString().c_str());
        const auto text = nt.getNogood(n).get();
        if (text == "")
            continue;
        const auto text_item = new QStandardItem(text.c_str());
        ng_model_->appendRow({nid_item, sid_item, text_item});
    }

    lo->addWidget(ng_table);

    connect(ng_table, &QTableView::doubleClicked, [this](const QModelIndex &idx) {
        const auto nid = ng_model_->item(idx.row())->text().toInt();
        emit nogoodClicked(NodeID(nid));
    });
}
} // namespace cpprofiler