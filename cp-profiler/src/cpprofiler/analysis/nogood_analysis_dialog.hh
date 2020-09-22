#pragma once

#include <QDialog>
#include <QStandardItemModel>
#include <QSortFilterProxyModel>
#include <QTableView>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QPushButton>

#include <QFile>
#include <QFileDialog>

#include <memory>

#include "../core.hh"

namespace cpprofiler
{

namespace analysis
{

struct NgAnalysisItem
{
    NogoodID nid;                    /// node id of the nogood
    const Nogood &ng;                /// textual representation of the nogood
    int total_red;                   /// total reduction by this nogood
    int count;                       /// number of times the nogood found in a 1-n pentagon
    std::vector<int> constraint_ids; /// reasons for the nogood
};

using NgAnalysisData = std::vector<NgAnalysisItem>;

class NogoodProxyModel : public QSortFilterProxyModel
{

    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override
    {
        /// TODO: extend this to work for clauses too (although this doesn't crash)
        int lhs = sourceModel()->data(left).toInt();
        int rhs = sourceModel()->data(right).toInt();

        return lhs < rhs;
    }
};

class NogoodAnalysisDialog : public QDialog
{
    Q_OBJECT

  private:
    std::unique_ptr<QStandardItemModel> ng_model_;

    QTableView *ng_table_;

    NgAnalysisData ng_data_;

    void init()
    {
        static constexpr int DEFAULT_WIDTH = 1000;
        static constexpr int DEFAULT_HEIGHT = 600;

        resize(DEFAULT_WIDTH, DEFAULT_HEIGHT);
        auto layout = new QVBoxLayout(this);

        ng_table_ = new QTableView();

        ng_table_->setSortingEnabled(true);

        layout->addWidget(ng_table_);

        ng_model_.reset(new QStandardItemModel(0, 4));

        auto proxy_model = new NogoodProxyModel();
        proxy_model->setSourceModel(ng_model_.get());

        const QStringList headers{"NodeID", "Total Reduction", "Count",
                                  "Clause"};
        ng_model_->setHorizontalHeaderLabels(headers);
        ng_table_->horizontalHeader()->setStretchLastSection(true);
        ng_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
        ng_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);

        ng_table_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
        ng_table_->setModel(proxy_model);

        connect(ng_table_, &QTableView::doubleClicked, [this](const QModelIndex &idx) {
            const auto nid = ng_model_->item(idx.row())->text().toInt();
            emit nogoodClicked(NodeID(nid));
        });

        auto save_ng_btn = new QPushButton("Save Nogoods");
        layout->addWidget(save_ng_btn, 0, Qt::AlignLeft);

        connect(save_ng_btn, &QPushButton::clicked, this, &NogoodAnalysisDialog::saveNogoods);
    }

    void saveNogoods()
    {
        QString file_name = QFileDialog::getSaveFileName(this, "Save nogoods to");
        /// No file was selected
        if (file_name.isEmpty())
            return;

        QFile file(file_name);

        if (!file.open(QIODevice::WriteOnly))
        {
            print("Error: could not open for writing: {}", file_name);
            return;
        }

        QTextStream nogood_stream(&file);
        const char sep = '\t';

        nogood_stream << "nid" << sep << "count" << sep << "reduction" << sep << "nogood" << sep << "reasons" << '\n';

        for (auto &ng_item : ng_data_)
        {
            nogood_stream << ng_item.nid << sep;
            nogood_stream << ng_item.count << sep;
            nogood_stream << ng_item.total_red << sep;

            nogood_stream << ng_item.ng.get().c_str() << sep;

            for (auto id : ng_item.constraint_ids)
            {
                nogood_stream << id << ' ';
            }

            nogood_stream << '\n';
        }
    }

  public:
    NogoodAnalysisDialog(NgAnalysisData nga_data) : QDialog(), ng_data_(std::move(nga_data))
    {
        init();
        populate(ng_data_);

        ng_table_->sortByColumn(1, Qt::SortOrder::DescendingOrder);
    }

    void populate(const NgAnalysisData &nga_data)
    {
        for (auto &item : nga_data)
        {
            const auto nid_i = new QStandardItem(QString::number(item.nid));
            const auto left_i = new QStandardItem(QString::number(item.total_red));
            const auto right_i = new QStandardItem(QString::number(item.count));
            const auto ng_i = new QStandardItem(item.ng.get().c_str());
            ng_model_->appendRow({nid_i, left_i, right_i, ng_i});
        }
    }

  signals:
    void nogoodClicked(NodeID nid);
};

} // namespace analysis

} // namespace cpprofiler