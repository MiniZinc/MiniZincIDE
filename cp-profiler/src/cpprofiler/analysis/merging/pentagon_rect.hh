#pragma once

#include <QGraphicsRectItem>
#include "../../core.hh"

namespace cpprofiler
{
namespace analysis
{

class PentagonListWidget;
struct PentagonItem;

namespace pent_config
{
constexpr int HEIGHT = 16;
constexpr int PADDING = 0;
constexpr int VIEW_WIDTH = 150;
// constexpr int PENT_WIDTH = VIEW_WIDTH - PADDING * 2 - 3;
// constexpr int HALF_WIDTH = PENT_WIDTH / 2;

constexpr int TEXT_PAD = 10;

static QColor left_color{153, 204, 255};
static QColor right_color{255, 153, 204};

/// color for selected pentagon item
static QColor sel_color{150, 150, 150};
} // namespace pent_config

class PentagonRect : public QGraphicsRectItem
{

  private:
    PentagonListWidget &m_pen_list_widget;
    /// Pentagon node associated with this item
    NodeID m_node;

    void mousePressEvent(QGraphicsSceneMouseEvent *) override;

  public:
    PentagonRect(QGraphicsScene *scene, PentagonListWidget &listw, const PentagonItem &pen, int y, int max_val, bool selected);
};

} // namespace analysis
} // namespace cpprofiler