#pragma once

#include <QScrollArea>
#include <QLabel>
#include <QWidget>

#include <memory>
#include <set>

#include "../core.hh"
#include "pixel_widget.hh"

namespace cpprofiler
{

namespace tree
{
class NodeTree;
}

namespace pixel_view
{

class PixelWidget;

struct PixelItem
{
    NodeID nid;
    int depth;
};

class PixelImage;

class PtCanvas : public QWidget
{
    Q_OBJECT
    const tree::NodeTree &tree_;

    std::unique_ptr<PixelImage> pimage_;

    std::unique_ptr<PixelWidget> pwidget_;

    /// Pixel Item DFS sequence
    std::vector<PixelItem> pi_seq_;

    /// the number of pixels per vertical line
    int compression_ = 2;

    /// which slices are currently selected
    std::set<int> selected_slices_;

  private:
    void drawPixelTree(bool all = false);


    std::vector<PixelItem> constructPixelTree() const;

  public:
    PtCanvas(const tree::NodeTree &tree);

    ~PtCanvas();

    PixelImage* get_pimage(void) const { return pimage_.get(); }
    void redrawAll(bool all = false);
    /// How many vertical slices does the tree span
    int totalSlices() const;
    void setCompression(int c) { compression_ = c; }

  signals:

    /// notify the traditional visualisation
    void nodesSelected(const std::vector<NodeID> &nodes);

  public slots:

    /// Select nodes based on vertical slices (may be out of bounds)
    void selectNodes(int vbegin, int vend);
};

} // namespace pixel_view
} // namespace cpprofiler
