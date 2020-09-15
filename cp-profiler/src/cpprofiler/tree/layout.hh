#ifndef CPPROFILER_TREE_LAYOUT_HH
#define CPPROFILER_TREE_LAYOUT_HH

#include <vector>
#include <memory>
#include <unordered_map>
#include <QMutex>
#include <QObject>

#include "../core.hh"
#include "shape.hh"

namespace cpprofiler
{
namespace tree
{

class Shape;
class LayoutComputer;
class Structure;
class BoundingBox;
class ShapeDeleter;

using ShapeUniqPtr = std::unique_ptr<Shape, ShapeDeleter>;

class Layout : public QObject
{
  Q_OBJECT

  mutable utils::Mutex layout_;

  /// TODO: make sure this is always protected by a mutex
  std::vector<ShapeUniqPtr> shapes_;

  /// Relative offset from the parent node along the x axis
  std::vector<double> child_offsets_;

  /// Whether layout for the node and its children is done (indexed by NodeID)
  std::vector<bool> layout_done_;

  /// Whether a node's shape need to be recomputed (indexed by NodeID)
  std::vector<bool> dirty_;

public:
  utils::Mutex &getMutex() const;

  void setChildOffset(NodeID nid, double offset) { child_offsets_[nid] = offset; }

  void setLayoutDone(NodeID nid, bool val) { layout_done_[nid] = val; }

  /// Note: a node might not have a shape (nullptr)
  /// if it was hidden before layout was run
  const Shape *getShape(NodeID nid) const { return shapes_[nid].get(); }

  void setShape(NodeID nid, ShapeUniqPtr shape);

  double getOffset(NodeID nid) const { return child_offsets_[nid]; }

  /// Get the height of the shape of node `nid`
  int getHeight(NodeID nid) const;

  /// Whether layout is done for the subtree associated with node `nid`;
  /// this is used by the drawing cursor to determine where to stop going down
  /// the tree
  bool getLayoutDone(NodeID nid) const;

  /// Whether layout is information for node exists
  bool ready(NodeID nid) const;

  /// Whether node `nid` is dirty (needs layout update)
  bool isDirty(NodeID nid) const { return dirty_[nid]; }

  /// Set node `nid` as (dirty) / (not dirty) based on `val`
  void setDirty(NodeID nid, bool val) { dirty_[nid] = val; }

  /// Get bounding box of node `nid`
  const BoundingBox &getBoundingBox(NodeID nid) const { return getShape(nid)->boundingBox(); }

  Layout();
  ~Layout();

public slots:

  void growDataStructures(int n_nodes);
};

} // namespace tree
} // namespace cpprofiler

#endif