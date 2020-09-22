#pragma once

#include <QAbstractScrollArea>

#include "../core.hh"

namespace cpprofiler
{
class UserData;
}

namespace cpprofiler
{
namespace tree
{

class NodeTree;
class Layout;
class VisualFlags;

struct DisplayState
{
    float scale;

    int root_x = 0;
    int root_y = 0;
};

class TreeScrollArea : public QAbstractScrollArea
{
    Q_OBJECT
    const NodeTree &m_tree;

    /// Current node in the traditional view
    const UserData &user_data_;
    const Layout &m_layout;

    DisplayState m_options;
    const VisualFlags &m_vis_flags;

    NodeID m_start_node;

    bool debug_mode_ = false;

    QPoint getNodeCoordinate(NodeID nid);
    NodeID findNodeClicked(int x, int y);

    void paintEvent(QPaintEvent *e) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

  signals:
    void nodeClicked(NodeID nid);
    void nodeDoubleClicked(NodeID nid);

  public:
    TreeScrollArea(NodeID start,
                   const NodeTree &,
                   const UserData &user_data,
                   const Layout &,
                   const VisualFlags &);

    /// center the x coordinate
    void centerPoint(int x, int y);

    void setDebugMode(bool val) { debug_mode_ = val; }

    void setScale(int val);

    void changeStartNode(NodeID nid);
};

} // namespace tree
} // namespace cpprofiler