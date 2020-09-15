#include "node.hh"

#include <cassert>
#include <iostream>
#include <exception>
#include <QDebug>

namespace cpprofiler
{
namespace tree
{

Node::Tag Node::getTag() const
{
    // obtain the right-most bit
    return static_cast<Tag>(reinterpret_cast<ptrdiff_t>(m_childrenOrFirstChild) & 3);
}

void Node::setTag(Tag tag)
{
    auto itag = static_cast<int>(tag);
    m_childrenOrFirstChild = reinterpret_cast<void *>(
        (reinterpret_cast<ptrdiff_t>(m_childrenOrFirstChild) & ~(3)) | itag);
}

NodeID *Node::getPtr() const
{
    return reinterpret_cast<NodeID *>(
        reinterpret_cast<ptrdiff_t>(m_childrenOrFirstChild) & ~(3));
}

void Node::setPtr(NodeID *ptr)
{
    m_childrenOrFirstChild = reinterpret_cast<void *>(ptr);
}

int Node::childrenCount() const
{

    auto tag = getTag();

    switch (tag)
    {
    case Tag::LEAF:
        return 0;
    case Tag::ONE_CHILD:
        return 1;
    case Tag::TWO_CHILDREN:
        return 2;
    default:
        return m_noOfChildren;
    }
}

void Node::setNumberOfChildren(int n)
{

    const auto old_n = childrenCount();

    /// Nothing to do
    if (old_n == n)
        return;

    /// This method works for "open" nodes only
    if (old_n != 0)
        throw std::exception();

    if (n == 1)
    {
        setTag(Tag::ONE_CHILD);
    }
    else if (n == 2)
    {
        setTag(Tag::TWO_CHILDREN);
    }
    else if (n > 2)
    {
        m_noOfChildren = n;
        setPtr(static_cast<NodeID *>(malloc(sizeof(NodeID) * n)));
        setTag(Tag::MORE_CHILDREN);
    }
}

Node::Node(NodeID parent_nid, int kids) : m_parent(parent_nid)
{
    // debug("node") << "    Node()\n";
    setTag(Tag::LEAF); /// initialize to be a leaf first
    setNumberOfChildren(kids);
}

void Node::setChild(NodeID nid, int alt)
{

    auto kids = childrenCount();

    if (kids <= 0)
    {
        throw no_child();
    }
    else if (kids <= 2)
    {
        if (alt == 0)
        {
            m_childrenOrFirstChild =
                reinterpret_cast<void *>(static_cast<int>(nid) << 2);
            setNumberOfChildren(kids);
        }
        else
        {
            if (alt != 1)
            {
                throw std::exception();
            }
            m_noOfChildren = -static_cast<int>(nid);
        }
    }
    else
    {
        getPtr()[alt] = nid;
    }

    auto kids2 = childrenCount();

    if (kids != kids2)
        throw;
}

NodeID Node::getChild(int alt) const
{
    const auto kids = childrenCount();

    if (kids <= 0)
    {
        throw std::exception();
    }
    else if (kids <= 2)
    {
        if (alt == 0)
        {
            return static_cast<NodeID>(static_cast<int>(reinterpret_cast<ptrdiff_t>(m_childrenOrFirstChild) >> 2));
        }
        else
        {
            return NodeID{-m_noOfChildren};
        }
    }
    else
    {
        return getPtr()[alt];
    }
}

void Node::addChild()
{

    auto kids = childrenCount();

    switch (kids)
    {
    case 0:
    {
        setTag(Tag::ONE_CHILD);
    }
    break;
    case 1:
    {
        setTag(Tag::TWO_CHILDREN);
    }
    break;
    case 2:
    {
        /// copy existing children
        // auto kids = new NodeID[3];
        auto kids = static_cast<NodeID *>(malloc(sizeof(NodeID) * 3));
        kids[0] = getChild(0);
        kids[1] = getChild(1);
        setPtr(kids);
        setTag(Tag::MORE_CHILDREN);
        m_noOfChildren = 3;
    }
    break;
    default:
    {
        m_noOfChildren++;
        auto ptr = static_cast<NodeID *>(realloc(getPtr(), sizeof(NodeID) * m_noOfChildren));
        setPtr(ptr);
        setTag(Tag::MORE_CHILDREN);
    }
    break;
    }
}

NodeID Node::getParent() const
{
    return m_parent;
}

void Node::removeChild(int alt)
{
    auto kids = childrenCount();

    /// Note: only works for two kids for now
    if (kids == 2)
    {
        /// move second child to first position
        if (alt == 0)
        {
            const auto rkid = getChild(1);
            setChild(rkid, 0);
        }

        setTag(Tag::ONE_CHILD);
    }
}

Node::~Node()
{
    if (getTag() == Tag::MORE_CHILDREN)
    {
        free(getPtr());
    }
}

QDebug &&operator<<(QDebug &&out, NodeStatus status)
{
    switch (status)
    {
    case NodeStatus::SOLVED:
    {
        out << "SOLVED";
    }
    break;
    case NodeStatus::FAILED:
    {
        out << "FAILED";
    }
    break;
    case NodeStatus::BRANCH:
    {
        out << "BRANCH";
    }
    break;
    case NodeStatus::SKIPPED:
    {
        out << "SKIPPED";
    }
    break;
    case NodeStatus::UNDETERMINED:
    {
        out << "UNDETERMINED";
    }
    break;
    }
    return std::move(out);
}

} // namespace tree
} // namespace cpprofiler