#ifndef CPPROFILER_TREE_NODE
#define CPPROFILER_TREE_NODE

#include <vector>
#include "../core.hh"

namespace cpprofiler
{
namespace tree
{

struct no_child : std::exception
{
    const char *what() const throw() override
    {
        return "child does not exist at alt\n";
    }
};

QDebug &&operator<<(QDebug &&out, NodeStatus status);

class Node
{
  private:
    enum class Tag
    {
        LEAF = 0,
        ONE_CHILD,
        TWO_CHILDREN,
        MORE_CHILDREN
    };

    /// The parent of this node
    NodeID m_parent;

    /// The children, or in case there are at most two, the first child
    void *m_childrenOrFirstChild;

    // The number of children, in case it is greater than 2, or the second child (if negative)
    int m_noOfChildren;

    Tag getTag() const;
    void setTag(Tag);

    /// Return children ptr (if nkids > 2) without the tag
    NodeID *getPtr() const;

    /// Set children ptr (if kids > 2) reseting the tag
    void setPtr(NodeID *ptr);

  public:
    Node(NodeID pid, int kids = 0);

    /// Return the number of immediate children
    int childrenCount() const;

    /// Allocate memory for children, possibly moving existing children
    void setNumberOfChildren(int n);

    /// Assign nid as the child at position alt
    void setChild(NodeID nid, int alt);

    /// Make room in the node for another child
    void addChild();

    /// Get the identifier of the child at position alt
    NodeID getChild(int alt) const;

    /// Get the identifier of the parent
    NodeID getParent() const;

    /// Remove `alt` child
    void removeChild(int alt);

    ~Node();
    Node &operator=(const Node &) = delete;
    Node(const Node &) = delete;
};

} // namespace tree
} // namespace cpprofiler

#endif