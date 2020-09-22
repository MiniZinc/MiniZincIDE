#pragma once

#include <QWidget>
#include <QDebug>
#include <QPainter>
#include "../config.hh"
#include "../core.hh"

#include "node_drawing.hh"

namespace cpprofiler
{
namespace tree
{

/// Draws a single node
class NodeWidget : public QWidget
{

    constexpr static int WIDTH = traditional::MAX_NODE_W + 4;

    NodeStatus m_status;

  protected:
    void paintEvent(QPaintEvent *) override
    {
        using namespace traditional;

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        switch (m_status)
        {
        case NodeStatus::SOLVED:
        {
            auto cy = (WIDTH - SOL_WIDTH) / 2;
            auto cx = cy + HALF_SOL_W;
            draw::solution(painter, cx, cy, false);
        }
        break;
        case NodeStatus::FAILED:
        {
            auto cy = (WIDTH - FAILED_WIDTH) / 2;
            auto cx = cy + HALF_FAILED_WIDTH;
            draw::failure(painter, cx, cy, false);
        }
        break;
        case NodeStatus::SKIPPED:
        {
            auto cy = (WIDTH - SKIPPED_WIDTH) / 2;
            auto cx = cy + HALF_SKIPPED_WIDTH;
            draw::skipped(painter, cx, cy, false);
        }
        break;
        case NodeStatus::BRANCH:
        {
            auto cy = (WIDTH - BRANCH_WIDTH) / 2;
            auto cx = cy + HALF_BRANCH_W;
            draw::branch(painter, cx, cy, false);
        }
        break;
        case NodeStatus::UNDETERMINED:
        {
            auto cy = (WIDTH - UNDET_WIDTH) / 2;
            auto cx = cy + HALF_UNDET_WIDTH;
            draw::unexplored(painter, cx, cy, false);
        }
        break;
        case NodeStatus::MERGED:
        {
            auto cy = (WIDTH - PENTAGON_WIDTH) / 2;
            auto cx = cy + PENTAGON_HALF_W;
            draw::pentagon(painter, cx, cy, false);
        }
        break;
        default:
            break;
        }
    }

  public:
    explicit NodeWidget(NodeStatus s) : m_status(s)
    {
        setMinimumSize(WIDTH, WIDTH);
        setMaximumSize(WIDTH, WIDTH);
    }

    ~NodeWidget()
    {
    }
};

} // namespace tree
} // namespace cpprofiler