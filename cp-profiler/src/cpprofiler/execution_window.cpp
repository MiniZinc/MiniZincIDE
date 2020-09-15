#include "execution_window.hh"

#include "tree/traditional_view.hh"
#include "pixel_views/pt_canvas.hh"
#include "pixel_views/icicle_canvas.hh"

#include "execution.hh"
#include "user_data.hh"

#include <QGridLayout>
#include <QTableView>
#include <QDockWidget>
#include <QHeaderView>
#include <QStandardItemModel>
#include <QSlider>
#include <QSplitter>
#include <QDebug>
#include <QMenuBar>
#include <QTextEdit>
#include <QFile>
#include <QTimer>
#include <QStatusBar>

#include "tree/node_tree.hh"

#include "utils/maybe_caller.hh"

#include "analysis/similar_subtree_window.hh"

#include "stats_bar.hpp"

namespace cpprofiler
{

LanternMenu::LanternMenu() : QWidget()
{
    auto layout = new QHBoxLayout(this);

    slider_ = new QSlider(Qt::Horizontal);
    slider_->setRange(0, 100);
    layout->addWidget(slider_);

    auto label_desc = new QLabel("limit:");
    layout->addWidget(label_desc);

    auto label = new QLabel("0");
    layout->addWidget(label);
    label->setAlignment(Qt::AlignVCenter | Qt::AlignRight);
    label->setMinimumWidth(55);

    connect(slider_, &QSlider::valueChanged, [this, label](int val) {
        label->setText(QString::number(val));
        emit limit_changed(val);
    });
}

ExecutionWindow::ExecutionWindow(Execution &ex, QWidget* parent)
    : QMainWindow(parent), execution_(ex)
{
    const auto &tree = ex.tree();
    traditional_view_.reset(new tree::TraditionalView(tree, ex.userData(), ex.solver_data()));

    connect(traditional_view_.get(), &tree::TraditionalView::nodeSelected,
            this, &ExecutionWindow::nodeSelected);

    connect(traditional_view_.get(), &tree::TraditionalView::nogoodsClicked, this, &ExecutionWindow::nogoodsClicked);

    connect(this, &ExecutionWindow::nodeSelected, traditional_view_.get(), &tree::TraditionalView::setCurrentNode);

    maybe_caller_.reset(new utils::MaybeCaller(30));

    auto layout = new QGridLayout();

    statusBar()->showMessage("Ready");

    auto stats_bar = new NodeStatsBar(this, tree.node_stats());
    statusBar()->addPermanentWidget(stats_bar);

    resize(500, 700);

    {
        auto widget = new QWidget();
        setCentralWidget(widget);
        widget->setLayout(layout);
    }

    layout->addWidget(traditional_view_->widget(), 0, 0, 2, 1);

    {

        auto sb = new QSlider(Qt::Vertical);

        sb->setMinimum(1);
        sb->setMaximum(100);

        constexpr int start_scale = 50;
        sb->setValue(start_scale);
        layout->addWidget(sb, 1, 1);

        traditional_view_->setScale(start_scale);

        connect(sb, &QSlider::valueChanged,
                traditional_view_.get(), &tree::TraditionalView::setScale);

        connect(&execution_.tree(), &tree::NodeTree::structureUpdated,
                traditional_view_.get(), &tree::TraditionalView::setLayoutOutdated);

        connect(&execution_.tree(), &tree::NodeTree::failedSubtreeClosed, [this](NodeID n) {
            traditional_view_->hideNode(n);
        });

        {
            auto statsUpdateTimer = new QTimer(this);
            connect(statsUpdateTimer, &QTimer::timeout, stats_bar, &NodeStatsBar::update);
            statsUpdateTimer->start(16);
        }
    }

    {
        lantern_widget = new LanternMenu();
        layout->addWidget(lantern_widget, 2, 0);
        lantern_widget->hide();

        // connect(lantern_widget, &LanternMenu::limit_changed,
        //         traditional_view_.get(), &tree::TraditionalView::hideBySize);
        connect(lantern_widget, &LanternMenu::limit_changed, [this](int limit) {
            maybe_caller_->call([this, limit]() {
                traditional_view_->hideBySize(limit);
            });
        });
    }

    {
        auto menuBar = new QMenuBar(0);
// Don't add the menu bar on Mac OS X
#ifndef Q_WS_MAC
        /// is this needed???
        setMenuBar(menuBar);
#endif

        {
            auto nodeMenu = menuBar->addMenu("&Node");

            auto centerNode = new QAction{"Center current node", this};
            centerNode->setShortcut(QKeySequence("C"));
            nodeMenu->addAction(centerNode);
            connect(centerNode, &QAction::triggered, traditional_view_.get(), &tree::TraditionalView::centerCurrentNode);

            auto toggleShowLabel = new QAction{"Show labels down", this};
            toggleShowLabel->setShortcut(QKeySequence("L"));
            nodeMenu->addAction(toggleShowLabel);
            connect(toggleShowLabel, &QAction::triggered, traditional_view_.get(), &tree::TraditionalView::showLabelsDown);

            auto toggleShowLabelsUp = new QAction{"Show labels down", this};
            toggleShowLabelsUp->setShortcut(QKeySequence("Shift+L"));
            nodeMenu->addAction(toggleShowLabelsUp);
            connect(toggleShowLabelsUp, &QAction::triggered, traditional_view_.get(), &tree::TraditionalView::showLabelsUp);

            auto hideFailed = new QAction{"Hide failed", this};
            hideFailed->setShortcut(QKeySequence("F"));
            nodeMenu->addAction(hideFailed);
            connect(hideFailed, &QAction::triggered, traditional_view_.get(), &tree::TraditionalView::hideFailed);

            auto unhideAll = new QAction{"Unhide all below", this};
            unhideAll->setShortcut(QKeySequence("U"));
            nodeMenu->addAction(unhideAll);
            connect(unhideAll, &QAction::triggered, traditional_view_.get(), &tree::TraditionalView::unhideAllAtCurrent);

            auto toggleHighlighted = new QAction{"Toggle highlight subtree", this};
            toggleHighlighted->setShortcut(QKeySequence("Shift+H"));
            nodeMenu->addAction(toggleHighlighted);
            connect(toggleHighlighted, &QAction::triggered, traditional_view_.get(), &tree::TraditionalView::toggleHighlighted);

            auto toggleHidden = new QAction{"Toggle hide/unhide subtree", this};
            toggleHidden->setShortcut(QKeySequence("H"));
            nodeMenu->addAction(toggleHidden);
            connect(toggleHidden, &QAction::triggered, traditional_view_.get(), &tree::TraditionalView::toggleHidden);

            auto bookmarkNode = new QAction{"Add/remove bookmark", this};
            bookmarkNode->setShortcut(QKeySequence("Shift+B"));
            nodeMenu->addAction(bookmarkNode);
            connect(bookmarkNode, &QAction::triggered, traditional_view_.get(), &tree::TraditionalView::bookmarkCurrentNode);

            auto showNogoods = new QAction{"Show nogoods", this};
            showNogoods->setShortcut(QKeySequence("Shift+N"));
            nodeMenu->addAction(showNogoods);
            connect(showNogoods, &QAction::triggered, traditional_view_.get(), &tree::TraditionalView::showNogoods);

            auto showNodeInfo = new QAction{"Show node info", this};
            showNodeInfo->setShortcut(QKeySequence("i"));
            nodeMenu->addAction(showNodeInfo);
            connect(showNodeInfo, &QAction::triggered, traditional_view_.get(), &tree::TraditionalView::showNodeInfo);
        }

        {

            auto navMenu = menuBar->addMenu("Na&vigation");

            auto navRoot = new QAction{"Go to the root", this};
            navRoot->setShortcut(QKeySequence("R"));
            navMenu->addAction(navRoot);
            connect(navRoot, &QAction::triggered, traditional_view_.get(), &tree::TraditionalView::navRoot);

            auto navDown = new QAction{"Go to the left-most child", this};
            navDown->setShortcut(QKeySequence("Down"));
            navMenu->addAction(navDown);
            connect(navDown, &QAction::triggered, traditional_view_.get(), &tree::TraditionalView::navDown);

            auto navDownAlt = new QAction{"Go to the right-most child", this};
            navDownAlt->setShortcut(QKeySequence("Alt+Down"));
            navMenu->addAction(navDownAlt);
            connect(navDownAlt, &QAction::triggered, traditional_view_.get(), &tree::TraditionalView::navDownAlt);

            auto navUp = new QAction{"Go up", this};
            navUp->setShortcut(QKeySequence("Up"));
            navMenu->addAction(navUp);
            connect(navUp, &QAction::triggered, traditional_view_.get(), &tree::TraditionalView::navUp);

            auto navLeft = new QAction{"Go left", this};
            navLeft->setShortcut(QKeySequence("Left"));
            navMenu->addAction(navLeft);
            connect(navLeft, &QAction::triggered, traditional_view_.get(), &tree::TraditionalView::navLeft);

            auto navRight = new QAction{"Go right", this};
            navRight->setShortcut(QKeySequence("Right"));
            navMenu->addAction(navRight);
            connect(navRight, &QAction::triggered, traditional_view_.get(), &tree::TraditionalView::navRight);
        }

        {
            auto viewMenu = menuBar->addMenu("Vie&w");

            auto showPixelTree = new QAction{"Pixel Tree View", this};
            showPixelTree->setCheckable(true);
            showPixelTree->setShortcut(QKeySequence("Shift+P"));
            viewMenu->addAction(showPixelTree);
            connect(showPixelTree, &QAction::triggered, this, &ExecutionWindow::showPixelTree);

            auto showIcicleTree = new QAction{"Icicle Tree View", this};
            showIcicleTree->setCheckable(true);
            showIcicleTree->setShortcut(QKeySequence("Shift+I"));
            viewMenu->addAction(showIcicleTree);
            connect(showIcicleTree, &QAction::triggered, this, &ExecutionWindow::showIcicleTree);

            auto toggleLanternTree = new QAction{"Lantern Tree View", this};
            toggleLanternTree->setCheckable(true);
            toggleLanternTree->setShortcut(QKeySequence("Ctrl+L"));
            viewMenu->addAction(toggleLanternTree);

            connect(toggleLanternTree, &QAction::triggered, this, &ExecutionWindow::toggleLanternView);
        }

        {
            auto dataMenu = menuBar->addMenu("&Data");

            auto showBookmarks = new QAction{"Show bookmarks", this};
            dataMenu->addAction(showBookmarks);
            connect(showBookmarks, &QAction::triggered, this, &ExecutionWindow::showBookmarks);
        }

        {
            auto analysisMenu = menuBar->addMenu("&Analyses");
            auto similarSubtree = new QAction{"Similar Subtrees", this};
            similarSubtree->setShortcut(QKeySequence("Shift+S"));
            analysisMenu->addAction(similarSubtree);

            connect(similarSubtree, &QAction::triggered, [this, &ex]() {
                auto ssw = new analysis::SimilarSubtreeWindow(this, ex.tree());
                ssw->show();

                connect(ssw, &analysis::SimilarSubtreeWindow::should_be_highlighted, [this](const std::vector<NodeID> &nodes, bool hide_rest) {
                    traditional_view_->highlightSubtrees(nodes, hide_rest);
                });
            });

            auto saveSearch = new QAction{"Save Search (for replaying)", this};
            analysisMenu->addAction(saveSearch);
            connect(saveSearch, &QAction::triggered, [this]() { emit needToSaveSearch(&execution_); });
        }

#ifdef DEBUG_MODE
        {
            auto debugMenu = menuBar->addMenu("Debu&g");

            auto updateLayoutAction = new QAction{"Update layout", this};
            debugMenu->addAction(updateLayoutAction);
            connect(updateLayoutAction, &QAction::triggered, traditional_view_.get(), &tree::TraditionalView::updateLayout);

            auto dirtyNodesUp = new QAction{"Dirty Nodes Up", this};
            debugMenu->addAction(dirtyNodesUp);
            connect(dirtyNodesUp, &QAction::triggered, traditional_view_.get(), &tree::TraditionalView::dirtyCurrentNodeUp);

            auto getNodeInfo = new QAction{"Print node info", this};
            getNodeInfo->setShortcut(QKeySequence("I"));
            debugMenu->addAction(getNodeInfo);
            connect(getNodeInfo, &QAction::triggered, traditional_view_.get(), &tree::TraditionalView::printNodeInfo);

            auto removeNode = new QAction{"Remove node", this};
            removeNode->setShortcut(QKeySequence("Del"));
            debugMenu->addAction(removeNode);
            connect(removeNode, &QAction::triggered, this, &ExecutionWindow::removeSelectedNode);

            auto checkLayout = new QAction{"Check layout", this};
            debugMenu->addAction(checkLayout);
            connect(checkLayout, &QAction::triggered, traditional_view_.get(), &tree::TraditionalView::debugCheckLayout);

            auto debugMode = new QAction{"Debug mode (show NodeIDs)", this};
            debugMode->setCheckable(true);
            debugMode->setShortcut(QKeySequence("Shift+D"));
            debugMenu->addAction(debugMode);
            connect(debugMode, &QAction::triggered, traditional_view_.get(), &tree::TraditionalView::setDebugMode);
        }
#endif

        // auto debugText = new QTextEdit{this};
        // // debugText->setHeight(200);
        // debugText->setReadOnly(true);

        // layout->addWidget(debugText, 2, 0);
    }
}

ExecutionWindow::~ExecutionWindow() = default;

tree::TraditionalView &ExecutionWindow::traditional_view()
{
    return *traditional_view_;
}

void ExecutionWindow::showBookmarks() const
{

    auto b_window = new QDialog();
    b_window->setAttribute(Qt::WA_DeleteOnClose);

    auto lo = new QVBoxLayout(b_window);

    auto bm_table = new QTableView();

    auto bm_model = new QStandardItemModel(0, 2);
    QStringList headers{"NodeID", "Bookmark Text"};
    bm_model->setHorizontalHeaderLabels(headers);
    bm_table->horizontalHeader()->setStretchLastSection(true);

    bm_table->setModel(bm_model);

    {
        const auto &ud = execution_.userData();

        const auto nodes = ud.bookmarkedNodes();

        for (const auto n : nodes)
        {
            auto nid_item = new QStandardItem(QString::number(n));
            auto text = ud.getBookmark(n);
            auto text_item = new QStandardItem(text.c_str());
            bm_model->appendRow({nid_item, text_item});
        }
    }

    lo->addWidget(bm_table);

    b_window->show();

    // QTableView
}

/// TODO: this should only be active when the tree is done building
void ExecutionWindow::removeSelectedNode()
{

    auto nid = execution_.userData().getSelectedNode();
    if (nid == NodeID::NoNode)
        return;

    const auto pid = execution_.tree().getParent(nid);
    execution_.userData().setSelectedNode(pid);

    execution_.tree().removeNode(nid);

    if (pid != NodeID::NoNode)
    {
        traditional_view_->dirtyUp(pid);
    }

    traditional_view_->setLayoutOutdated();
    traditional_view_->redraw();
}

static void writeToFile(const QString &path, const QString &str)
{
    QFile file(path);

    if (file.open(QFile::WriteOnly | QFile::Truncate))
    {
        QTextStream out(&file);

        out << str;
    }
    else
    {
        qDebug() << "could not open the file: " << path;
    }
}

void ExecutionWindow::showPixelTree()
{
    const auto &tree = execution_.tree();

    /// Can only show pixel tree when the tree is fully built
    /// TODO: change this to allow partially built trees
    //if (!tree.isDone())
    //    return;

    if (!pt_dock_)
    {
        pt_dock_ = new QDockWidget("Pixel Tree", this);
        pt_dock_->setAllowedAreas(Qt::BottomDockWidgetArea);
        addDockWidget(Qt::BottomDockWidgetArea, pt_dock_);
        pixel_tree_.reset(new pixel_view::PtCanvas(tree));
        pt_dock_->setWidget(pixel_tree_.get());

        connect(pixel_tree_.get(), &pixel_view::PtCanvas::nodesSelected, [this](const std::vector<NodeID> &nodes) {
            traditional_view_->highlightSubtrees(nodes, true, false);
        });
    }

    if (pt_dock_->isHidden())
    {
        pt_dock_->show();
    }
    else
    {
        pt_dock_->hide();
    }
}

void ExecutionWindow::showIcicleTree()
{

    const auto &tree = execution_.tree();
    /// Can only show icicle tree when the tree is fully built
    /// TODO: change this to allow partially built trees
    //if (!tree.isDone())
    //    return;

    if (!it_dock_)
    {
        /// Create Icicle Tree Widget

        it_dock_ = new QDockWidget("Icicle Tree", this);
        it_dock_->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::TopDockWidgetArea);
        addDockWidget(Qt::TopDockWidgetArea, it_dock_);

        icicle_tree_.reset(new pixel_view::IcicleCanvas(tree));
        it_dock_->setWidget(icicle_tree_.get());

        connect(icicle_tree_.get(), &pixel_view::IcicleCanvas::nodeClicked, this, &ExecutionWindow::nodeSelected);
        connect(icicle_tree_.get(), &pixel_view::IcicleCanvas::nodeClicked, traditional_view_.get(), &tree::TraditionalView::centerCurrentNode);

        /// Note: nodeSelected can be triggered by some other view
        connect(this, &ExecutionWindow::nodeSelected, icicle_tree_.get(), &pixel_view::IcicleCanvas::selectNode);
    }

    if (it_dock_->isHidden())
    {
        it_dock_->show();
    }
    else
    {
        it_dock_->hide();
    }
}

void ExecutionWindow::toggleLanternView(bool checked)
{

    if (checked)
    {
        const int max_nodes = execution_.tree().nodeCount();
        lantern_widget->setMax(max_nodes);
        lantern_widget->show();
        auto old_value = lantern_widget->value();
        traditional_view_->hideBySize(old_value);
    }
    else
    {
        /// Undo lantern tree
        traditional_view_->undoLanterns();
        traditional_view_->unhideAll();
        lantern_widget->hide();
    }
}

void ExecutionWindow::print_log(const std::string &str)
{
    writeToFile("debug.log", str.c_str());
}

} // namespace cpprofiler
