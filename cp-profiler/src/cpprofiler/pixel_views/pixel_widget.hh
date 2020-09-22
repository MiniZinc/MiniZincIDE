#pragma once

#include <QAbstractScrollArea>
#include <QPainter>
#include <QResizeEvent>

#include <cmath> // std::ceil

#include "pixel_image.hh"
#include "../utils/debug.hh"

namespace cpprofiler
{

namespace pixel_view
{

class PixelWidget : public QAbstractScrollArea
{
    Q_OBJECT
    const PixelImage &image_;

    /// record previosly clicked vline
    int pressed_vline_ = -1;

    static constexpr int PADDING = 5;

  protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter{viewport()};
        painter.drawImage(QPoint(PADDING, PADDING), image_.raw_image());
    }

    void resizeEvent(QResizeEvent *e) override
    {
        viewport_resized(e->size());
    }

    void mousePressEvent(QMouseEvent *e) override;

    void mouseReleaseEvent(QMouseEvent *e) override;

  public:
    PixelWidget(const PixelImage &image) : image_(image) {}

    /// How many "pixels" fit in one page;
    int width() const
    {

        return static_cast<int>(std::ceil(static_cast<float>(viewport()->width() - 10) / image_.pixel_size()));
    }

  signals:
    void viewport_resized(const QSize &size);

    /// notify pixel canvas that some slices have been selected;
    /// indexes may refer to non-existing slices
    void range_selected(int x_l, int x_r);

    /// Notify that (x, y) is clicked in absolute values
    void coordinate_clicked(int x, int y);
};

} // namespace pixel_view

} // namespace cpprofiler
