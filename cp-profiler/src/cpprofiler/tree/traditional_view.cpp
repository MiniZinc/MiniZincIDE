#include "traditional_view.hh"
#include "tree_scroll_area.hh"
#include "layout.hh"
#include "structure.hh"
#include "node_tree.hh"

#include <queue>
#include <thread>
#include <cmath>

#include <QPainter>
#include <QDebug>
#include <QTimer>
#include <QScrollBar>
#include <QMouseEvent>
#include <QInputDialog>
#include <QLineEdit>
#include <QTextEdit>
#include <QThread>
#include <QVBoxLayout>

#include "cursors/nodevisitor.hh"
#include "cursors/hide_failed_cursor.hh"
#include "cursors/hide_not_highlighted_cursor.hh"
#include "../utils/std_ext.hh"
#include "node_id.hh"
#include "shape.hh"
#include "../user_data.hh"
#include "../solver_data.hh"
#include "layout_computer.hh"
#include "../config.hh"

#include "../nogood_dialog.hh"

#include "../utils/perf_helper.hh"
#include "../utils/tree_utils.hh"

namespace cpprofiler
{
namespace tree
{

TraditionalView::TraditionalView(const NodeTree &tree, UserData &ud, SolverData &sd)
    : tree_(tree),
      user_data_(ud),
      solver_data_(sd),
      vis_flags_(utils::make_unique<VisualFlags>()),
      layout_(utils::make_unique<Layout>()),
      layout_computer_(utils::make_unique<LayoutComputer>(tree, *layout_, *vis_flags_))
{
    utils::DebugMutexLocker tree_lock(&tree_.treeMutex());

    scroll_area_.reset(new TreeScrollArea(tree_.getRoot(), tree_, user_data_, *layout_, *vis_flags_));

    // std::cerr << "traditional view thread:" << std::this_thread::get_id() << std::endl;

    // connect(scroll_area_.get(), &TreeScrollArea::nodeClicked, this, &TraditionalView::setCurrentNode);
    connect(scroll_area_.get(), &TreeScrollArea::nodeClicked, this, &TraditionalView::nodeSelected);
    connect(scroll_area_.get(), &TreeScrollArea::nodeDoubleClicked, this, &TraditionalView::handleDoubleClick);

    connect(this, &TraditionalView::needsRedrawing, this, &TraditionalView::redraw);
    connect(this, &TraditionalView::needsLayoutUpdate, this, &TraditionalView::updateLayout);

    connect(&tree, &NodeTree::childrenStructureChanged, [this](NodeID nid) {
        if (nid == NodeID::NoNode)
        {
            return;
        }
        layout_computer_->dirtyUpLater(nid);
        // layout_->setLayoutDone(nid, false);
    });

    auto autoLayoutTimer = new QTimer(this);

    connect(autoLayoutTimer, &QTimer::timeout, this, &TraditionalView::autoUpdate);

    /// stop this timer up when the tree is finished?
    autoLayoutTimer->start(100);
}

TraditionalView::~TraditionalView() = default;

void TraditionalView::redraw()
{
    scroll_area_->viewport()->update();
}

NodeID TraditionalView::node() const
{
    return user_data_.getSelectedNode();
}

void TraditionalView::setNode(NodeID nid)
{
    user_data_.setSelectedNode(nid);
}

void TraditionalView::navRoot()
{
    auto root = tree_.getRoot();
    emit nodeSelected(root);
    centerCurrentNode(); /// TODO: this should be needed
    emit needsRedrawing();
}

void TraditionalView::navDown()
{

    const auto nid = node();

    if (nid == NodeID::NoNode)
        return;

    utils::DebugMutexLocker tree_lock(&tree_.treeMutex());

    const auto kids = tree_.childrenCount(nid);

    if (kids == 0 || vis_flags_->isHidden(nid))
        return;

    auto first_kid = tree_.getChild(nid, 0);

    emit nodeSelected(first_kid);
    centerCurrentNode();
    emit needsRedrawing();
}

void TraditionalView::navDownAlt()
{
    const auto nid = node();
    if (nid == NodeID::NoNode)
        return;

    utils::DebugMutexLocker tree_lock(&tree_.treeMutex());

    const auto kids = tree_.childrenCount(nid);

    if (kids == 0 || vis_flags_->isHidden(nid))
        return;

    auto last_kid = tree_.getChild(nid, kids - 1);
    emit nodeSelected(last_kid);
    centerCurrentNode();
    emit needsRedrawing();
}

void TraditionalView::navUp()
{
    auto nid = node();
    if (nid == NodeID::NoNode)
        return;

    utils::DebugMutexLocker tree_lock(&tree_.treeMutex());

    auto pid = tree_.getParent(nid);

    if (pid != NodeID::NoNode)
    {
        emit nodeSelected(pid);
        centerCurrentNode();
        emit needsRedrawing();
    }
}

void TraditionalView::navLeft()
{
    auto nid = node();
    if (nid == NodeID::NoNode)
        return;

    utils::DebugMutexLocker tree_lock(&tree_.treeMutex());

    auto pid = tree_.getParent(nid);
    if (pid == NodeID::NoNode)
        return;

    auto cur_alt = tree_.getAlternative(nid);

    if (cur_alt > 0)
    {
        auto kid = tree_.getChild(pid, cur_alt - 1);
        emit nodeSelected(kid);
        centerCurrentNode();
        emit needsRedrawing();
    }
}

void TraditionalView::navRight()
{
    /// lock mutex
    auto nid = node();
    if (nid == NodeID::NoNode)
        return;

    utils::DebugMutexLocker tree_lock(&tree_.treeMutex());

    auto pid = tree_.getParent(nid);

    if (pid == NodeID::NoNode)
        return;

    auto cur_alt = tree_.getAlternative(nid);

    auto kids = tree_.childrenCount(pid);

    if (cur_alt + 1 < kids)
    {
        const auto kid = tree_.getChild(pid, cur_alt + 1);
        emit nodeSelected(kid);
        centerCurrentNode();
        emit needsRedrawing();
    }
}

void TraditionalView::setLabelShown(NodeID nid, bool val)
{
    vis_flags_->setLabelShown(nid, val);
    layout_computer_->dirtyUpLater(nid);
}

void TraditionalView::toggleShowLabel()
{
    auto nid = node();
    if (nid == NodeID::NoNode)
        return;

    auto val = !vis_flags_->isLabelShown(nid);
    setLabelShown(nid, val);
    emit needsRedrawing();
    layout_computer_->dirtyUpLater(nid);
}

void TraditionalView::showLabelsDown()
{
    auto nid = node();
    if (nid == NodeID::NoNode)
        return;

    auto val = !vis_flags_->isLabelShown(nid);

    utils::pre_order_apply(tree_, nid, [val, this](NodeID nid) {
        setLabelShown(nid, val);
    });

    emit needsLayoutUpdate();
    emit needsRedrawing();
}

void TraditionalView::showLabelsUp()
{
    auto nid = node();
    if (nid == NodeID::NoNode)
        return;

    auto pid = tree_.getParent(nid);

    /// if it is root, toggle for the root only
    if (pid == NodeID::NoNode)
    {
        toggleShowLabel();
        return;
    }

    auto val = !vis_flags_->isLabelShown(pid);

    while (nid != NodeID::NoNode)
    {
        setLabelShown(nid, val);
        nid = tree_.getParent(nid);
    }

    emit needsLayoutUpdate();
    emit needsRedrawing();
}

static bool is_leaf(const NodeTree &nt, NodeID nid)
{
    return nt.childrenCount(nid) == 0;
}

void TraditionalView::hideNode(NodeID n, bool delayed)
{
    utils::DebugMutexLocker tree_lock(&tree_.treeMutex());
    utils::DebugMutexLocker layout_lock(&layout_->getMutex());

    if (is_leaf(tree_, n))
        return;

    vis_flags_->setHidden(n, true);

    dirtyUp(n);

    if (delayed)
    {
        setLayoutOutdated();
    }
    else
    {
        emit needsLayoutUpdate();
    }
    emit needsRedrawing();
}

void TraditionalView::toggleHidden()
{
    auto nid = node();
    if (nid == NodeID::NoNode)
        return;

    // do not hide leaf nodes
    if (is_leaf(tree_, nid))
        return;

    auto val = !vis_flags_->isHidden(nid);
    vis_flags_->setHidden(nid, val);

    dirtyUp(nid);

    emit needsLayoutUpdate();
    emit needsRedrawing();
}

void TraditionalView::hideFailedAt(NodeID n, bool onlyDirty)
{
    /// Do nothing if there is no tree
    if (tree_.nodeCount() == 0)
        return;

    bool modified = false;

    HideFailedCursor hfc(n, tree_, *vis_flags_, *layout_computer_, onlyDirty, modified);
    PostorderNodeVisitor<HideFailedCursor>(hfc).run();

    if (modified)
    {
        emit needsLayoutUpdate();
        emit needsRedrawing();
    }
}

void TraditionalView::hideFailed(bool onlyDirty)
{
    auto nid = node();
    if (nid == NodeID::NoNode)
        return;

    hideFailedAt(nid, onlyDirty);
}

void TraditionalView::autoUpdate()
{
    if (!layout_stale_)
        return;

    const auto changed = updateLayout();
    if (changed)
    {
        emit needsRedrawing();
    }
}

void TraditionalView::handleDoubleClick()
{

    auto nid = node();
    if (nid == NodeID::NoNode)
        return;

    auto status = tree_.getStatus(nid);

    if (status == NodeStatus::BRANCH)
    {
        unhideNode(nid);
    }
    else if (status == NodeStatus::MERGED)
    {
        toggleCollapsePentagon(nid);
    }

    /// should this happen automatically whenever the layout is changed?
    centerCurrentNode();
}

void TraditionalView::toggleCollapsePentagon(NodeID nid)
{
    /// Use the same 'hidden' flag for now
    auto val = !vis_flags_->isHidden(nid);
    vis_flags_->setHidden(nid, val);
    dirtyUp(nid);
    emit needsLayoutUpdate();
    emit needsRedrawing();
}

void TraditionalView::setNodeHidden(NodeID n, bool val)
{
    vis_flags_->setHidden(n, false);
    layout_->setLayoutDone(n, false);
}

void TraditionalView::unhideNode(NodeID nid)
{
    utils::DebugMutexLocker tree_lock(&tree_.treeMutex());
    utils::DebugMutexLocker layout_lock(&layout_->getMutex());

    auto hidden = vis_flags_->isHidden(nid);
    if (hidden)
    {
        setNodeHidden(nid, false);

        dirtyUp(nid);
        emit needsLayoutUpdate();
        emit needsRedrawing();
    }
}

void TraditionalView::bookmarkCurrentNode()
{
    auto nid = node();

    if (!user_data_.isBookmarked(nid))
    {
        /// Add bookmark
        bool accepted;
        auto text = QInputDialog::getText(nullptr, "Add bookmark", "Name:", QLineEdit::Normal, "", &accepted);
        if (!accepted)
            return;

        user_data_.setBookmark(nid, text.toStdString());
    }
    else
    {
        /// Remove bookmark
        user_data_.clearBookmark(nid);
    }

    emit needsRedrawing();
}

void TraditionalView::unhideAllAt(NodeID n)
{
    utils::DebugMutexLocker tree_lock(&tree_.treeMutex());
    utils::DebugMutexLocker layout_lock(&layout_->getMutex());

    if (n == tree_.getRoot())
    {
        unhideAll();
        return;
    }

    /// indicates if any change was made
    bool modified = false;

    const auto action = [&](NodeID n) {
        if (vis_flags_->isHidden(n))
        {
            vis_flags_->setHidden(n, false);
            layout_->setLayoutDone(n, false);
            dirtyUp(n);
            modified = true;
        }
    };

    utils::apply_below(tree_, n, action);

    if (modified)
    {
        emit needsLayoutUpdate();
        emit needsRedrawing();
    }

    centerCurrentNode();
}

void TraditionalView::unhideAll()
{

    /// faster version for the entire tree
    if (vis_flags_->hiddenCount() == 0)
    {
        return;
    }

    for (auto n : vis_flags_->hidden_nodes())
    {
        dirtyUp(n);
        layout_->setLayoutDone(n, false);
    }

    vis_flags_->unhideAll();

    emit needsLayoutUpdate();
    emit needsRedrawing();
    centerNode(tree_.getRoot());
}

void TraditionalView::unhideAllAtCurrent()
{
    auto nid = node();
    if (nid == NodeID::NoNode)
        return;

    unhideAllAt(nid);
}

void TraditionalView::toggleHighlighted()
{
    auto nid = node();
    if (nid == NodeID::NoNode)
        return;

    auto val = !vis_flags_->isHighlighted(nid);
    vis_flags_->setHighlighted(nid, val);

    emit needsRedrawing();
}

QWidget *TraditionalView::widget()
{
    return scroll_area_.get();
}

const Layout &TraditionalView::layout() const
{
    return *layout_;
}

void TraditionalView::setScale(int val)
{
    scroll_area_->setScale(val);
}

/// relative to the root
static int global_node_x_offset(const NodeTree &tree, const Layout &layout, NodeID nid)
{
    auto x_off = 0;

    while (nid != NodeID::NoNode)
    {
        x_off += layout.getOffset(nid);
        nid = tree.getParent(nid);
    }

    return x_off;
}

/// Does this need any locking?
void TraditionalView::centerNode(NodeID nid)
{

    const auto x_offset = global_node_x_offset(tree_, *layout_, nid);

    const auto root_nid = tree_.getRoot();
    const auto bb = layout_->getBoundingBox(root_nid);

    const auto value_x = x_offset - bb.left;

    const auto depth = utils::calculate_depth(tree_, nid);
    const auto value_y = depth * layout::dist_y;

    scroll_area_->centerPoint(value_x, value_y);
}

void TraditionalView::centerCurrentNode()
{
    centerNode(node());
}

void TraditionalView::setCurrentNode(NodeID nid)
{
    user_data_.setSelectedNode(nid);
    emit needsRedrawing();
}

void TraditionalView::setAndCenterNode(NodeID nid)
{
    setCurrentNode(nid);
    centerNode(nid);
}

bool TraditionalView::updateLayout()
{
    const auto changed = layout_computer_->compute();
    layout_stale_ = false;

    return changed;
}

void TraditionalView::setLayoutOutdated()
{
    layout_stale_ = true;
}

void TraditionalView::dirtyUp(NodeID nid)
{
    layout_computer_->dirtyUpLater(nid);
}

void TraditionalView::dirtyCurrentNodeUp()
{
    const auto nid = node();
    if (nid == NodeID::NoNode)
        return;

    dirtyUp(nid);
}

void TraditionalView::printNodeInfo()
{
    const auto nid = node();
    if (nid == NodeID::NoNode)
        return;

    print("--- Node Info: {} ----", nid);
    print("offset: {}", layout_->getOffset(nid));
    auto bb = layout_->getBoundingBox(nid);
    print("bb:[{},{}]", bb.left, bb.right);
    print("dirty: {}", layout_->isDirty(nid));
    print("layout done for node: {}", layout_->getLayoutDone(nid));
    print("hidden: {}", vis_flags_->isHidden(nid));
    print("total kids: {}, ", tree_.childrenCount(nid));
    print("has solved kids: {}, ", tree_.hasSolvedChildren(nid));
    print("has open kids: {}", tree_.hasOpenChildren(nid));

    const auto &ng = tree_.getNogood(nid);

    if (ng.has_renamed())
    {
        print("nogood: {} ({})", ng.renamed(), ng.original());
    }
    else
    {
        print("nogood: {}", ng.original());
    }
    print("alt: {}", tree_.getAlternative(nid));
}

/// Show specified subtrees and hide everyting else
static void showSubtrees(const NodeTree &tree, VisualFlags &vf, LayoutComputer &lc)
{
    auto root = tree.getRoot();

    HideNotHighlightedCursor hnhc(root, tree, vf, lc);
    PostorderNodeVisitor<HideNotHighlightedCursor>(hnhc).run();
}

class TreeHighlighter : public QThread
{

    const NodeTree &tree_;
    VisualFlags &vf_;
    Layout &layout_;
    LayoutComputer &lc_;

  public:
    TreeHighlighter(const NodeTree &tree, VisualFlags &vf, Layout &lo, LayoutComputer &lc)
        : tree_(tree), vf_(vf), layout_(lo), lc_(lc) {}

    void run() override
    {
        utils::DebugMutexLocker t_locker(&tree_.treeMutex());
        utils::DebugMutexLocker l_locker(&layout_.getMutex());

        auto root = tree_.getRoot();

        HideNotHighlightedCursor hnhc(root, tree_, vf_, lc_);
        PostorderNodeVisitor<HideNotHighlightedCursor>(hnhc).run();
    }
};

void TraditionalView::revealNode(NodeID n)
{
    utils::DebugMutexLocker t_locker(&tree_.treeMutex());
    utils::DebugMutexLocker l_locker(&layout_->getMutex());

    layout_computer_->dirtyUpUnconditional(n);

    while (n != NodeID::NoNode)
    {
        setNodeHidden(n, false);
        n = tree_.getParent(n);
    }

    emit needsLayoutUpdate();
    emit needsRedrawing();
}

void TraditionalView::highlightSubtrees(const std::vector<NodeID> &nodes, bool hide_rest, bool show_outline)
{
    vis_flags_->unhighlightAll();

    detail::PerformanceHelper phelper;

    for (auto nid : nodes)
    {
        vis_flags_->setHighlighted(nid, true);
    }

    if (hide_rest)
    {
        unhideAll();

        auto root = tree_.getRoot();
        HideNotHighlightedCursor hnhc(root, tree_, *vis_flags_, *layout_computer_);
        PostorderNodeVisitor<HideNotHighlightedCursor>(hnhc).run();

        emit needsLayoutUpdate();
    }

    /// Note: this duplication could probably be avoided
    if (!show_outline)
    {
        for (auto nid : nodes)
        {
            vis_flags_->setHighlighted(nid, false);
        }
    }

    emit needsRedrawing();
}

/// Lantern Tree Visualisation
void TraditionalView::hideBySize(int size_limit)
{
    utils::DebugMutexLocker tree_lock(&tree_.treeMutex());

    unhideAll();

    vis_flags_->resetLanternSizes();

    const int max_lantern = 127;

    const auto sizes = utils::calc_subtree_sizes(tree_);

    const auto root = tree_.getRoot();

    std::stack<NodeID> stack;

    stack.push(root);

    while (!stack.empty())
    {
        const auto n = stack.top();
        stack.pop();

        const auto size = sizes.at(n);
        const auto nkids = tree_.childrenCount(n);

        if (size > size_limit)
        {
            /// visit children
            for (auto alt = 0u; alt < nkids; ++alt)
            {
                stack.push(tree_.getChild(n, alt));
            }
        }
        else
        {
            /// turn the node into a "lantern"
            if (nkids > 0)
            {
                vis_flags_->setHidden(n, true);
                /// lantern size
                auto lsize = (size * max_lantern) / size_limit;
                vis_flags_->setLanternSize(n, lsize);
                layout_->setLayoutDone(n, false);
                dirtyUp(n);
            }
        }
    }

    emit needsLayoutUpdate();
    emit needsRedrawing();

    centerCurrentNode();
}

void TraditionalView::undoLanterns()
{
    vis_flags_->resetLanternSizes();
}

void TraditionalView::showNodeInfo() const
{

    if (!solver_data_.hasInfo())
        return;

    const auto cur_nid = node();
    if (cur_nid == NodeID::NoNode)
        return;

    auto info_dialog = new QDialog;
    auto layout = new QVBoxLayout(info_dialog);

    QTextEdit* te = new QTextEdit;
    te->append(solver_data_.getInfo(cur_nid).c_str());

    layout->addWidget(te);

    info_dialog->setAttribute(Qt::WA_DeleteOnClose);
    info_dialog->show();
}

void TraditionalView::showNogoods() const
{

    if (!solver_data_.hasNogoods())
        return;

    const auto cur_nid = node();
    if (cur_nid == NodeID::NoNode)
        return;

    const auto nodes = utils::nodes_below(tree_, cur_nid);

    auto ng_dialog = new NogoodDialog(tree_, nodes);
    ng_dialog->setAttribute(Qt::WA_DeleteOnClose);

    connect(ng_dialog, &NogoodDialog::nogoodClicked, [this](NodeID nid) {
        const_cast<TraditionalView *>(this)->revealNode(nid);
        const_cast<TraditionalView *>(this)->setAndCenterNode(nid);
        emit nogoodsClicked({nid});
    });

    ng_dialog->show();
}

void TraditionalView::debugCheckLayout() const
{
    ///
    const auto order = utils::any_order(tree_);

    for (const auto n : order)
    {
        auto bb = layout_->getBoundingBox(n);
        // print("bb for {} is fine", n);
    }
}

void TraditionalView::setDebugMode(bool v)
{
    scroll_area_->setDebugMode(true);
    layout_computer_->setDebugMode(true);
    emit needsLayoutUpdate();
    emit needsRedrawing();
}

} // namespace tree
} // namespace cpprofiler
