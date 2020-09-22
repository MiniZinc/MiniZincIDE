#pragma once

#include <QWidget>
#include <QGraphicsView>
#include <QVBoxLayout>

#include "merge_result.hh"

#include "pentagon_rect.hh"

namespace cpprofiler
{
namespace analysis
{

class PentagonListWidget : public QWidget
{
    Q_OBJECT

    /// Elements for drawing primitives
    QGraphicsView *view_;
    QGraphicsScene *scene_;

    /// Data to be visualised
    const MergeResult &merge_res_;

    /// Whether pentagon list should be sorted (by right/left difference)
    bool needs_sorting = true;

    /// Which pentagon from the list is selected
    NodeID selected_ = NodeID::NoNode;

  public:
    /// Create specifying parent widget and the result of merging
    PentagonListWidget(QWidget *parent, const MergeResult &res);

    /// Clear the view and draw horizontal bars indicating pentagons
    void updateScene();

    /// Width available for drawing
    int viewWidth() { return view_->viewport()->width(); }

    /// Handle pentagon click
    void handleClick(NodeID node)
    {
        selected_ = node;
        emit pentagonClicked(node);
        updateScene();
    }

  signals:
    /// Indicate that a pentagon (associated with some NodeID) was clicked
    void pentagonClicked(NodeID);

  public slots:
    /// Handle checkbox click from Merge Window
    void handleSortCB(int state)
    {
        needs_sorting = (state == Qt::Checked);
        updateScene();
    }
};

inline PentagonListWidget::PentagonListWidget(QWidget *w, const MergeResult &res) : QWidget(w), merge_res_(res)
{

    auto layout = new QVBoxLayout(this);

    view_ = new QGraphicsView(this);
    view_->setAlignment(Qt::AlignLeft | Qt::AlignTop);

    view_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    view_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);

    view_->setMaximumWidth(pent_config::VIEW_WIDTH);
    view_->setMinimumWidth(pent_config::VIEW_WIDTH);

    scene_ = new QGraphicsScene(this);
    view_->setScene(scene_);

    layout->addWidget(view_);
}

inline void PentagonListWidget::updateScene()
{
    using namespace pent_config;

    scene_->clear();

    /// This is used to scale the horizontal bars
    int max_value = 0;
    for (const auto &pen : merge_res_)
    {
        max_value = std::max(max_value, std::max(pen.size_l, pen.size_r));
    }

    auto displayed_items = merge_res_; /// make copy

    const auto sort_function = [](const PentagonItem &p1, const PentagonItem &p2) {
        const auto diff1 = std::abs(p1.size_r - p1.size_l);
        const auto diff2 = std::abs(p2.size_r - p2.size_l);

        if (diff1 < diff2)
        {
            return false;
        }
        else if (diff2 > diff1)
        {
            return true;
        }
        else
        {
            const auto sum1 = p1.size_r + p1.size_l;
            const auto sum2 = p2.size_r + p2.size_l;
            return sum2 < sum1;
        }
    };

    if (needs_sorting)
    {
        std::sort(displayed_items.begin(), displayed_items.end(), sort_function);
    }

    for (auto i = 0; i < displayed_items.size(); ++i)
    {
        const auto ypos = i * (HEIGHT + PADDING) + PADDING;

        const bool sel = (displayed_items[i].pen_nid == selected_);
        new PentagonRect(scene_, *this, displayed_items[i], ypos, max_value, sel);
    }
}

} // namespace analysis
} // namespace cpprofiler