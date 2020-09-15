#include "pattern_rect.hh"
#include "histogram_scene.hh"

namespace cpprofiler
{
namespace analysis
{

static constexpr int SELECTION_WIDTH = 500;

PatternRect::PatternRect(HistogramScene &hist_scene, int x, int y, int width, int height)
    : QGraphicsRectItem(x, y - V_DISTANCE / 2, SELECTION_WIDTH, height + V_DISTANCE), visible_rect(x, y, width, height), m_hist_scene(hist_scene)
{
    QColor gold(252, 209, 22);
    setPen(Qt::NoPen);

    QColor patternRectColor(255, 215, 179);
    visible_rect.setBrush(patternRectColor);
}

void PatternRect::mousePressEvent(QGraphicsSceneMouseEvent *)
{
    m_hist_scene.findAndSelect(this);
}

void PatternRect::setHighlighted(bool val)
{

    QPen pen;
    if (val)
    {
        pen.setWidth(3);
        pen.setBrush(highlighted_outline);
    }
    else
    {
        // pen.setBrush(normal_outline);
        pen.setStyle(Qt::NoPen);
    }
    setPen(pen);
}

void PatternRect::addToScene()
{
    m_hist_scene.scene()->addItem(&visible_rect);
    m_hist_scene.scene()->addItem(this);
}

} // namespace analysis
} // namespace cpprofiler