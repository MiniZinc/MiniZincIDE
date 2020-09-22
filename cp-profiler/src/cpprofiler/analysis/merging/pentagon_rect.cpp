#include "pentagon_rect.hh"

#include "merge_result.hh"
#include "pentagon_list_widget.hh"

#include <QGraphicsRectItem>
#include <QGraphicsScene>

namespace cpprofiler
{
namespace analysis
{

PentagonRect::PentagonRect(QGraphicsScene *scene, PentagonListWidget &listw, const PentagonItem &pen, int y, int max_val, bool selected)
    : QGraphicsRectItem(pent_config::PADDING, y, listw.viewWidth() - 1, pent_config::HEIGHT),
      m_pen_list_widget(listw),
      m_node(pen.pen_nid)
{
    using namespace pent_config;

    const int PENT_WIDTH = listw.viewWidth() - 1;
    const int HALF_WIDTH = PENT_WIDTH / 2;

    const float scale_x = (float)HALF_WIDTH / max_val;

    const int value_l = pen.size_l;
    const int value_r = pen.size_r;

    const int width_l = value_l * scale_x;
    const int width_r = value_r * scale_x;

    const int cx = PADDING + listw.viewWidth() / 2;

    if (selected)
    {
        setBrush(sel_color);
    }

    auto left = new QGraphicsRectItem(cx - width_l, y, width_l, HEIGHT);
    left->setBrush(left_color);
    // left->setPen(Qt::NoPen);
    auto right = new QGraphicsRectItem(cx, y, width_r, HEIGHT);
    right->setBrush(right_color);
    // right->setPen(Qt::NoPen);

    scene->addItem(this);
    scene->addItem(left);
    scene->addItem(right);

    {
        auto text_item = new QGraphicsSimpleTextItem{QString::number(value_l)};
        text_item->setPos(TEXT_PAD, y);
        scene->addItem(text_item);
    }

    {
        auto text_item = new QGraphicsSimpleTextItem{QString::number(value_r)};
        const int item_width = text_item->boundingRect().width();
        const int x = PENT_WIDTH - TEXT_PAD - item_width;
        text_item->setPos(x, y);
        scene->addItem(text_item);
    }
}

void PentagonRect::mousePressEvent(QGraphicsSceneMouseEvent *)
{
    m_pen_list_widget.handleClick(m_node);
}

} // namespace analysis
} // namespace cpprofiler