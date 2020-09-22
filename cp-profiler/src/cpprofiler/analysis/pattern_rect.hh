#pragma once

#include "../core.hh"
#include <QGraphicsRectItem>
#include <memory>

namespace cpprofiler
{
namespace analysis
{

class HistogramScene;

class PatternRect : public QGraphicsRectItem
{

    QGraphicsRectItem visible_rect;

    HistogramScene &m_hist_scene;

    void mousePressEvent(QGraphicsSceneMouseEvent *) override;

    static QColor normal_outline;
    static QColor highlighted_outline;

  public:
    PatternRect(HistogramScene &hist_scene, int x, int y, int width, int height);

    void setHighlighted(bool val);

    void addToScene();
};

} // namespace analysis
} // namespace cpprofiler