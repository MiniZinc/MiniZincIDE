#include "histogram_scene.hh"
#include "pattern_rect.hh"
#include "../utils/perf_helper.hh"

namespace cpprofiler
{
namespace analysis
{

static constexpr int SHAPE_RECT_HEIGHT = 16;
static constexpr int NUMBER_WIDTH = 50;
static constexpr int COLUMN_WIDTH = NUMBER_WIDTH + 10;

static constexpr int ROW_HEIGHT = SHAPE_RECT_HEIGHT + V_DISTANCE;

enum class Align
{
    CENTER,
    RIGHT
};

QColor PatternRect::normal_outline{252, 209, 22};
QColor PatternRect::highlighted_outline{252, 148, 77};

static void addText(QGraphicsScene &scene, int col, int row,
                    QGraphicsSimpleTextItem *text_item,
                    Align alignment = Align::CENTER)
{
    int item_width = text_item->boundingRect().width();
    int item_height = text_item->boundingRect().height();

    // center the item vertically at y
    int y_offset = item_height / 2;
    int x_offset = 0;

    switch (alignment)
    {
    case Align::CENTER:
        x_offset = (COLUMN_WIDTH - item_width) / 2;
        break;
    case Align::RIGHT:
        x_offset = COLUMN_WIDTH - item_width;
        break;
    }

    int x = col * COLUMN_WIDTH + x_offset;
    int y = row * ROW_HEIGHT + y_offset;

    text_item->setPos(x, y - y_offset);
    scene.addItem(text_item);
}

static std::shared_ptr<PatternRect> addRect(HistogramScene &hist_scene, int row, int val)
{

    int x = 0;
    int y = row * ROW_HEIGHT;

    int width = val * 40;

    auto item = std::make_shared<PatternRect>(hist_scene, x, y, width, SHAPE_RECT_HEIGHT);
    item->addToScene();

    return item;
}

static void addText(QGraphicsScene &scene, int col, int row, const char *text)
{
    auto str = new QGraphicsSimpleTextItem{text};
    addText(scene, col, row, str);
}

static void addText(QGraphicsScene &scene, int col, int row, int value)
{
    auto int_text_item = new QGraphicsSimpleTextItem{QString::number(value)};
    addText(scene, col, row, int_text_item, Align::RIGHT);
}

void HistogramScene::drawPatterns(PatternProp prop)
{

    auto &scene = *scene_;

    addText(scene, 0, 0, "hight");
    addText(scene, 1, 0, "count");
    addText(scene, 2, 0, "size");

    int row = 1;

    for (auto &&p : patterns_)
    {

        const auto nid = p->first();
        const int count = p->count();
        const int height = p->height();
        /// number of nodes in the frist subtree represeting a pattern
        const auto size = p->size();

        int val;
        switch (prop)
        {
        case PatternProp::HEIGHT:
            val = height;
            break;
        case PatternProp::COUNT:
            val = count;
            break;
        case PatternProp::SIZE:
            val = size;
            break;
        }

        auto item = addRect(*this, row, val);

        rects_.push_back(item);
        rect2pattern_.insert(std::make_pair(item, p));

        addText(scene, 0, row, height);
        addText(scene, 1, row, count);
        addText(scene, 2, row, size);

        ++row;
    }
}

void HistogramScene::setPatterns(std::vector<SubtreePattern> &&patterns)
{
    patterns_.reserve(patterns.size());

    for (auto &p : patterns)
    {
        patterns_.push_back(std::make_shared<SubtreePattern>(std::move(p)));
    }
}

int HistogramScene::findPatternIdx(PatternRect *pattern) const
{
    for (auto i = 0; i < rects_.size(); ++i)
    {
        if (pattern == rects_[i].get())
        {
            return i;
        }
    }

    return -1;
}

PatternPtr HistogramScene::rectToPattern(PatternRectPtr prect) const
{
    if (rect2pattern_.find(prect) != rect2pattern_.end())
    {
        return rect2pattern_.at(prect);
    }
    return nullptr;
}

void HistogramScene::reset()
{

    if (selected_rect_)
    {
        selected_rect_->setHighlighted(false);
        selected_rect_ = nullptr;
        selected_idx_ = -1;
    }

    rects_.clear();
    rect2pattern_.clear();
    scene_->clear();
    patterns_.clear();
}

void HistogramScene::changeSelectedPattern(int idx)
{

    if (idx < 0 || idx >= rects_.size())
        return;

    auto prect = rects_[idx];

    /// Need to find pattern to get the list of nodes
    auto pattern = rectToPattern(prect);
    if (!pattern)
        return;

    emit pattern_selected(pattern->first());
    emit should_be_highlighted(pattern->nodes());

    {
        if (idx == selected_idx_)
        {
            selected_rect_->setHighlighted(false);
            selected_idx_ = -1;
            selected_rect_ = nullptr;
        }
        else
        {
            selected_idx_ = idx;
            /// unselect the old one
            if (selected_rect_)
            {
                selected_rect_->setHighlighted(false);
            }
            prect->setHighlighted(true);
            selected_idx_ = idx;
            selected_rect_ = prect.get();
        }
    }
}

void HistogramScene::findAndSelect(PatternRect *prect)
{
    auto idx = findPatternIdx(prect);

    changeSelectedPattern(idx);
}

/// TODO: this should change scroll bar value as well
void HistogramScene::prevPattern()
{
    if (rects_.size() == 0)
        return;

    auto new_idx = selected_idx_ - 1;

    if (new_idx < 0)
        return;

    changeSelectedPattern(new_idx);
}

void HistogramScene::nextPattern()
{
    if (rects_.size() == 0)
        return;

    auto new_idx = selected_idx_ + 1;

    if (new_idx >= rects_.size())
        return;

    changeSelectedPattern(new_idx);
}

} // namespace analysis
} // namespace cpprofiler