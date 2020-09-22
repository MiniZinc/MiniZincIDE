#include "pt_canvas.hh"
#include "pixel_image.hh"
#include "pixel_widget.hh"
#include "../tree/node_tree.hh"

#include "../utils/perf_helper.hh"
#include "../utils/debug.hh"

#include <cmath>     // std::ceil
#include <algorithm> // std::min

#include <QPainter>
#include <QPushButton>
#include <QScrollBar>
#include <QVBoxLayout>

namespace cpprofiler
{

namespace pixel_view
{

namespace colors
{
QRgb solution = qRgb(50, 230, 50);
} // namespace colors

PtCanvas::PtCanvas(const tree::NodeTree &tree) : QWidget(), tree_(tree)
{
    pimage_.reset(new PixelImage());
    pwidget_.reset(new PixelWidget(*pimage_));
    pimage_->setPixelSize(4);

    pi_seq_ = constructPixelTree();

    const auto max_depth = tree_.node_stats().maxDepth();

    auto layout = new QVBoxLayout(this);
    layout->addWidget(pwidget_.get());

    auto controlLayout = new QHBoxLayout();
    layout->addLayout(controlLayout);

    controlLayout->addWidget(new QLabel("Zoom:"));

    {
        auto zoomOut = new QPushButton("-", this);
        zoomOut->setMaximumWidth(40);
        controlLayout->addWidget(zoomOut);
        connect(zoomOut, &QPushButton::clicked, [this]() {
            pimage_->zoomOut();
            redrawAll();
        });
    }

    {
        auto zoomIn = new QPushButton("+", this);
        zoomIn->setMaximumWidth(40);
        controlLayout->addWidget(zoomIn);
        connect(zoomIn, &QPushButton::clicked, [this]() {
            pimage_->zoomIn();
            redrawAll();
        });
    }

    controlLayout->addStretch();

    {

        controlLayout->addWidget(new QLabel("Compression:"));

        auto addCompression = new QPushButton("-", this);
        auto reduceCompression = new QPushButton("+", this);

        addCompression->setMaximumWidth(40);
        controlLayout->addWidget(addCompression);
        connect(addCompression, &QPushButton::clicked, [reduceCompression, this]() {
            compression_ += 10;
            reduceCompression->setEnabled(true);
            redrawAll();
        });

        reduceCompression->setMaximumWidth(40);
        reduceCompression->setEnabled(false);
        controlLayout->addWidget(reduceCompression);
        connect(reduceCompression, &QPushButton::clicked, [reduceCompression, this]() {
            compression_ = std::max(1, compression_ - 10);

            if (compression_ == 1)
                reduceCompression->setEnabled(false);

            redrawAll();
        });
    }

    connect(pwidget_.get(), &PixelWidget::viewport_resized, [this](const QSize &size) {
        pimage_->resize(size);
        redrawAll();
    });

    connect(pwidget_->horizontalScrollBar(), &QScrollBar::valueChanged, [this]() {
        redrawAll();
    });

    connect(pwidget_.get(), &PixelWidget::range_selected, this, &PtCanvas::selectNodes);
    connect(pwidget_.get(), &PixelWidget::coordinate_clicked, [this](int x, int) {
        selectNodes(x, x);
    });

    redrawAll();
}

PtCanvas::~PtCanvas() = default;

int PtCanvas::totalSlices() const
{
    return std::ceil((float)pi_seq_.size() / compression_);
}

std::vector<PixelItem> PtCanvas::constructPixelTree() const
{

    print("pt: construct tree");
    /// TODO: tree mutex
    auto root = tree_.getRoot();

    std::stack<NodeID> stack;
    std::stack<int> depth_stack;

    std::vector<PixelItem> pixel_seq;
    pixel_seq.reserve(tree_.nodeCount());

    stack.push(root);
    depth_stack.push(1);

    while (!stack.empty())
    {

        auto n = stack.top();
        stack.pop();

        auto depth = depth_stack.top();
        depth_stack.pop();

        /// add pixel data
        pixel_seq.push_back({n, depth});

        int kids = tree_.childrenCount(n);

        for (auto i = kids - 1; i >= 0; --i)
        {
            stack.push(tree_.getChild(n, i));
            depth_stack.push(depth + 1);
        }
    }

    stack.push(root);

    return pixel_seq;
}

void PtCanvas::redrawAll(bool all)
{
    pimage_->clear();

    drawPixelTree(all);

    {
        const auto total_width = totalSlices();
        /// how many "pixels" fit in one page
        const auto page_width = pwidget_->width();

        /// Note: page width is 1 smaller for the purpose of calculating the srollbar range,
        /// than it is for drawing (this way some "pixels" can be drawn partially at the edge)
        pwidget_->horizontalScrollBar()->setRange(0, total_width - (page_width - 1));
        pwidget_->horizontalScrollBar()->setPageStep(page_width);
    }

    pimage_->update();
    pwidget_->viewport()->update();
}

void PtCanvas::drawPixelTree(bool all)
{

    static int times_called = 0;

    times_called++;

    // print("draw pixel tree: {}", times_called);

    /// which vertical slice to draw at x = 0
    const auto v_begin = all ? 0 : pwidget_->horizontalScrollBar()->value();
    /// how many slices are visible
    const auto visible_slices = pwidget_->width();
    const auto v_end = all ? totalSlices() : v_begin + visible_slices;

    bool end_reached = false;

    for (auto slice = v_begin; slice < v_end && !end_reached; ++slice)
    {
        int x = slice - v_begin;
        int first_idx = slice * compression_;

        /// is silce selected?
        bool selected = selected_slices_.find(slice) != selected_slices_.end();

        QRgb color = qRgb(30, 40, 30);

        if (selected)
        {
            color = qRgb(255, 0, 0);
        }

        /// See if there is a solution node (warning: duplication / bad code!)
        /// (Note that this has to be separate from drawing, as the solution line
        /// should go behind the actual nodes)
        bool has_solutions = false;
        for (auto idx = first_idx; idx < first_idx + compression_; ++idx)
        {
            if (idx == pi_seq_.size())
            {
                end_reached = true;
                break;
            }
            const auto node = pi_seq_[idx].nid;

            if (tree_.getStatus(node) == tree::NodeStatus::SOLVED)
            {
                has_solutions = true;
                break;
            }
        }

        if (has_solutions)
        {
            for (auto y = 0; y < tree_.depth(); ++y)
            {
                pimage_->drawPixel(x, y, colors::solution);
            }
        }

        /// Draw a "slice"
        for (auto idx = first_idx; idx < first_idx + compression_; ++idx)
        {
            if (idx == pi_seq_.size())
            {
                end_reached = true;
                break;
            }
            const auto &pi = pi_seq_[idx];
            const int y = pi.depth;
            pimage_->drawPixel(x, y, color);
        }
    }
}

void PtCanvas::selectNodes(int vbegin, int vend)
{

    if (vend < vbegin)
    {
        return;
    }

    selected_slices_.clear();

    vend = std::min(totalSlices() - 1, vend);

    for (auto slice = vbegin; slice <= vend; ++slice)
    {
        selected_slices_.insert(slice);
    }

    /// recover the nodes from selected slices
    std::vector<NodeID> selected_nodes;
    for (auto slice : selected_slices_)
    {
        int first_idx = slice * compression_;

        for (auto idx = first_idx; idx < first_idx + compression_; ++idx)
        {
            if (idx == pi_seq_.size())
                break;

            selected_nodes.push_back(pi_seq_[idx].nid);
        }
    }

    emit nodesSelected(selected_nodes);

    redrawAll();
}

} // namespace pixel_view
} // namespace cpprofiler
