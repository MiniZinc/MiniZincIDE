#ifndef CPPROFILER_TREE_SHAPE
#define CPPROFILER_TREE_SHAPE

#include "../utils/array.hh"
#include <ostream>
#include <initializer_list>

namespace cpprofiler
{
namespace tree
{

/// Shape's extent on some level
class Extent
{
  public:
    /// Left extent
    int l;
    /// Right extent
    int r;
    /// Construct with \a l and \a r
    Extent(int l0, int r0) : l(l0), r(r0) {}

    Extent() = default;
};

/// Shape's bounding box
struct BoundingBox
{
    int left;
    int right;

    int width() const
    {
        return right - left;
    }
};

/// Subtree's shape (outline) represented by extents on each depth level
class Shape
{

    /// Shape's extents
    utils::Array<Extent> extents_;

    /// Shapes's bounding box
    BoundingBox bb_;

    friend std::ostream &operator<<(std::ostream &, const Shape &);

  public:
    /// Create a shape of depth/height of `height` with extents uninitialized
    explicit Shape(int height);

    /// Create a shape using initializer list and a pre-computed bounding box
    Shape(std::initializer_list<Extent> init_list, const BoundingBox &bb)
        : extents_{init_list}, bb_{bb} {}

    /// Get the depth/height of the shape
    int height() const { return extents_.size(); }

    /// Get the extent at `depth`
    Extent &operator[](int depth) { return extents_[depth]; }

    /// Get the extent at `depth`
    const Extent &operator[](int depth) const { return extents_[depth]; }

    /// Set bounding box
    void setBoundingBox(BoundingBox bb) { bb_ = bb; }

    /// Get bounding box
    const BoundingBox &boundingBox() const { return bb_; }

    /// Shape used for all leaf nodes (that don't have labels displayed)
    static Shape leaf;
    /// Shape used for all hidden nodes / failed subtrees (that don't have labels displayed)
    static Shape hidden;
};

class ShapeDeleter
{
  public:
    void operator()(Shape *s)
    {
        if (s != &Shape::leaf && s != &Shape::hidden)
        {
            delete s;
        }
    }
};

} // namespace tree
} // namespace cpprofiler

#endif