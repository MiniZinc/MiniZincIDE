#include "pixel_widget.hh"

#include <QScrollBar>

namespace cpprofiler
{

namespace pixel_view
{

void PixelWidget::mousePressEvent(QMouseEvent *e)
{
    const auto xoff = horizontalScrollBar()->value();
    const auto yoff = verticalScrollBar()->value();

    const int abs_x = (e->x() - PADDING) / image_.pixel_size() + xoff;
    const int abs_y = (e->y() - PADDING) / image_.pixel_size() + yoff;

    pressed_vline_ = abs_x;

    emit coordinate_clicked(abs_x, abs_y);
}

void PixelWidget::mouseReleaseEvent(QMouseEvent *e)
{
    const auto xoff = horizontalScrollBar()->value();

    const int vline = (e->x() - PADDING) / image_.pixel_size() + xoff;

    if (pressed_vline_ < vline)
        emit range_selected(pressed_vline_, vline);
    else if (pressed_vline_ > vline)
        emit range_selected(vline, pressed_vline_);
}

} // namespace pixel_view
} // namespace cpprofiler