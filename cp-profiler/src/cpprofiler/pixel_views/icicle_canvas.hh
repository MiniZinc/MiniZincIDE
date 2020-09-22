#pragma once

#include <QWidget>
#include <memory>

#include "../core.hh"

namespace cpprofiler
{

namespace utils
{
class MaybeCaller;
}

namespace tree
{
class NodeTree;
}

namespace pixel_view
{

class PixelImage;
class PixelWidget;
class IcicleLayout;

class IcicleCanvas : public QWidget
{
    Q_OBJECT
    const tree::NodeTree &tree_;

    std::unique_ptr<utils::MaybeCaller> maybe_caller_;
    std::unique_ptr<PixelImage> pimage_;
    std::unique_ptr<PixelWidget> pwidget_;

    std::unique_ptr<IcicleLayout> layout_;

    /// Currently selected node;
    NodeID selected_;

    /// The last "generation" of nodes displayed (1 being leaf nodes)
    /// Note: the default of 1 requires the '+' button to be disabled
    int compression_ = 1;

  public:
    IcicleCanvas(const tree::NodeTree &tree);

    ~IcicleCanvas();

    void redrawAll();

    void drawIcicleTree();

  public slots:

    /// highlight the node on the view; to be called via signals/slots only!
    void selectNode(NodeID n);

  signals:
    void nodeClicked(NodeID n);
};
} // namespace pixel_view
} // namespace cpprofiler