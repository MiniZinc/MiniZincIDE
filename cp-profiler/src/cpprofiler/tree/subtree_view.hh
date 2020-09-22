#pragma once

#include <QWidget>

#include "../user_data.hh"
#include "tree_scroll_area.hh"
#include "layout_computer.hh"
#include "visual_flags.hh"
#include "layout.hh"
#include "../core.hh"

namespace cpprofiler
{
namespace tree
{

class SubtreeView : public QWidget
{
    Q_OBJECT

    std::unique_ptr<UserData> user_data_;
    std::unique_ptr<Layout> m_layout;
    std::unique_ptr<TreeScrollArea> m_scroll_area;

    /// Not really used in this view?
    VisualFlags node_flags;

    /// Current node representing the root of the subtree
    NodeID node_ = NodeID::NoNode;

  public:
    SubtreeView(const NodeTree &nt)
        : user_data_(utils::make_unique<UserData>()), m_layout(utils::make_unique<Layout>())
    {
        auto layout_computer = LayoutComputer(nt, *m_layout, node_flags);
        layout_computer.compute();

        m_scroll_area.reset(new TreeScrollArea(node_, nt, *user_data_, *m_layout, node_flags));
        m_scroll_area->setScale(50);
        m_scroll_area->viewport()->update();
    }

    QWidget *widget()
    {
        return m_scroll_area.get();
    }

  public slots:

    void setNode(NodeID nid)
    {
        node_ = nid;
        m_scroll_area->changeStartNode(nid);
        m_scroll_area->viewport()->update();
    }
};

} // namespace tree
} // namespace cpprofiler