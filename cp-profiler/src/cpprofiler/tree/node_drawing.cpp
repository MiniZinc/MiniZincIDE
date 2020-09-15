#include "node_drawing.hh"
#include "../config.hh"

#include <QPainter>

namespace cpprofiler
{
namespace tree
{
namespace draw
{

namespace colors
{
/// The color for selected nodes
static QColor gold(252, 209, 22);
/// Red color for failed nodes
static QColor red(218, 37, 29);
/// Green color for solved nodes
static QColor green(11, 118, 70);
/// Blue color for choice nodes
static QColor blue(0, 92, 161);
/// Grey color for skipped nodes
static QColor grey(150, 150, 150);
/// Orange color for best solutions
static QColor orange(235, 137, 27);
/// White color
static QColor white(255, 255, 255);

/// Red color for expanded failed nodes
static QColor lightRed(218, 37, 29, 120);
/// Green color for expanded solved nodes
static QColor lightGreen(11, 118, 70, 120);
/// Blue color for expanded choice nodes
static QColor lightBlue(0, 92, 161, 120);
/// Color for pentagons (orange)
static QColor pentagonColor(235, 137, 27);
} // namespace colors

static void drawDiamond(QPainter &painter, int myx, int myy, bool shadow)
{
    using namespace traditional;
    QPointF points[4] = {QPointF(myx, myy),
                         QPointF(myx + HALF_SOL_W, myy + HALF_SOL_W),
                         QPointF(myx, myy + SOL_WIDTH),
                         QPointF(myx - HALF_SOL_W, myy + HALF_SOL_W)};

    if (shadow)
    {
        for (auto &&p : points)
        {
            p += QPointF(SHADOW_OFFSET, SHADOW_OFFSET);
        }
    }

    painter.drawConvexPolygon(points, 4);
}

void solution(QPainter &painter, int x, int y, bool selected)
{
    using namespace traditional;
    if (selected)
    {
        painter.setBrush(colors::gold);
    }
    else
    {
        painter.setBrush(colors::green);
    }

    drawDiamond(painter, x, y, false);
}

void failure(QPainter &painter, int x, int y, bool selected)
{
    using namespace traditional;
    if (selected)
    {
        painter.setBrush(colors::gold);
    }
    else
    {
        painter.setBrush(colors::red);
    }

    painter.drawRect(x - HALF_FAILED_WIDTH, y, FAILED_WIDTH, FAILED_WIDTH);
}

void branch(QPainter &painter, int x, int y, bool selected)
{

    using namespace traditional;
    if (selected)
    {
        painter.setBrush(colors::gold);
    }
    else
    {
        painter.setBrush(colors::blue);
    }

    painter.drawEllipse(x - HALF_BRANCH_W, y, BRANCH_WIDTH, BRANCH_WIDTH);
}

void unexplored(QPainter &painter, int x, int y, bool selected)
{
    using namespace traditional;
    if (selected)
    {
        painter.setBrush(colors::gold);
    }
    else
    {
        painter.setBrush(colors::white);
    }

    painter.drawEllipse(x - HALF_UNDET_WIDTH, y, UNDET_WIDTH, UNDET_WIDTH);
}

void skipped(QPainter &painter, int x, int y, bool selected)
{
    using namespace traditional;
    if (selected)
    {
        painter.setBrush(colors::gold);
    }
    else
    {
        painter.setBrush(colors::grey);
    }

    painter.drawRect(x - HALF_SKIPPED_WIDTH, y, SKIPPED_WIDTH, SKIPPED_WIDTH);
}

void pentagon(QPainter &painter, int x, int y, bool selected)
{
    using namespace traditional;
    if (selected)
    {
        painter.setBrush(colors::gold);
    }
    else
    {
        painter.setBrush(colors::pentagonColor);
    }

    QPointF points[5] = {QPointF(x, y),
                         QPointF(x + PENTAGON_HALF_W, y + PENTAGON_THIRD_W),
                         QPointF(x + PENTAGON_THIRD_W, y + PENTAGON_WIDTH),
                         QPointF(x - PENTAGON_THIRD_W, y + PENTAGON_WIDTH),
                         QPointF(x - PENTAGON_HALF_W, y + PENTAGON_THIRD_W)};

    painter.drawConvexPolygon(points, 5);
}

void big_pentagon(QPainter &painter, int x, int y, bool selected)
{
    using namespace traditional;
    if (selected)
    {
        painter.setBrush(colors::gold);
    }
    else
    {
        painter.setBrush(colors::pentagonColor);
    }

    QPointF points[5] = {QPointF(x, y),
                         QPointF(x + BIG_PENTAGON_HALF_W, y + BIG_PENTAGON_THIRD_W),
                         QPointF(x + BIG_PENTAGON_THIRD_W, y + BIG_PENTAGON_WIDTH),
                         QPointF(x - BIG_PENTAGON_THIRD_W, y + BIG_PENTAGON_WIDTH),
                         QPointF(x - BIG_PENTAGON_HALF_W, y + BIG_PENTAGON_THIRD_W)};

    painter.drawConvexPolygon(points, 5);
}

void lantern(QPainter &painter, int x, int y, int size, bool selected, bool has_gradient, bool has_solutions)
{

    using namespace lantern;

    const int height = K * size;

    if (selected)
    {
        painter.setBrush(colors::gold);
    }
    else
    {
        QColor main_color = has_solutions ? colors::green : colors::red;
        if (has_gradient)
        {
            QLinearGradient gradient(x - HALF_WIDTH, y,
                                     x + HALF_WIDTH, y + BASE_HEIGHT + height);
            gradient.setColorAt(0, colors::white);
            gradient.setColorAt(1, main_color);
            painter.setBrush(gradient);
        }
        else
        {
            painter.setBrush(main_color);
        }
    }

    QPointF points[5] = {QPointF(x, y),
                         QPointF(x + HALF_WIDTH, y + BASE_HEIGHT),
                         QPointF(x + HALF_WIDTH, y + BASE_HEIGHT + height),
                         QPointF(x - HALF_WIDTH, y + BASE_HEIGHT + height),
                         QPointF(x - HALF_WIDTH, y + BASE_HEIGHT)};

    painter.drawConvexPolygon(points, 5);
}

} // namespace draw
} // namespace tree
} // namespace cpprofiler