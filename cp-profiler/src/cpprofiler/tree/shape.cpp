#include "shape.hh"
#include "../config.hh"
#include <QDebug>
#include <ostream>

namespace cpprofiler
{
namespace tree
{

Shape::Shape(int height) : extents_(height) {}

std::ostream &operator<<(std::ostream &os, const cpprofiler::tree::Shape &s)
{
    os << "{ height: " << s.height() << ", [ ";

    for (auto i = 0; i < s.extents_.size(); ++i)
    {
        os << "{" << s.extents_[i].l << ":" << s.extents_[i].r << "} ";
    }

    return os << "]}";
}

using namespace traditional;

Shape Shape::leaf({{-HALF_MAX_NODE_W, HALF_MAX_NODE_W}},
                  BoundingBox{-HALF_MAX_NODE_W, HALF_MAX_NODE_W});

Shape Shape::hidden({{-HALF_MAX_NODE_W, HALF_MAX_NODE_W}, {-MAX_NODE_W, MAX_NODE_W}},
                    BoundingBox{-MAX_NODE_W, MAX_NODE_W});
} // namespace tree
} // namespace cpprofiler
