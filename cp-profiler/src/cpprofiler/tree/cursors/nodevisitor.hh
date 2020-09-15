#ifndef CPPROFILER_TREE_CURSORS_NODE_VISITOR_HH
#define CPPROFILER_TREE_CURSORS_NODE_VISITOR_HH

namespace cpprofiler
{
namespace tree
{

template <typename Cursor>
class NodeVisitor
{

  protected:
    Cursor m_cursor;

  public:
    NodeVisitor(const Cursor &c);
};

template <typename Cursor>
class PreorderNodeVisitor : public NodeVisitor<Cursor>
{

    using NodeVisitor<Cursor>::m_cursor;

  protected:
    /// Move cursor to next node from a leaf
    bool backtrack();
    /// Move cursor to the next node, return true if succeeded
    bool next();

  public:
    PreorderNodeVisitor(const Cursor &c);
    /// Execute visitor
    void run();
};

template <typename Cursor>
class PostorderNodeVisitor : public NodeVisitor<Cursor>
{

    using NodeVisitor<Cursor>::m_cursor;

  protected:
    /// Move the cursor to the left-most leaf
    void moveToLeaf();

  public:
    /// Constructor
    PostorderNodeVisitor(const Cursor &c);
    /// Move cursor to next node, return true if succeeded
    bool next();
    /// Execute visitor
    void run();
};

#include "nodevisitor.hpp"

} // namespace tree
} // namespace cpprofiler

#endif
