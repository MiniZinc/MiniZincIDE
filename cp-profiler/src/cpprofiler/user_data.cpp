#include "user_data.hh"

namespace cpprofiler
{

void UserData::setSelectedNode(tree::NodeID nid)
{
    m_selected_node = nid;
}

tree::NodeID UserData::getSelectedNode() const
{
    return m_selected_node;
}

void UserData::setBookmark(tree::NodeID nid, const std::string &text)
{
    bookmarks_.insert({nid, text});
}

const std::string &UserData::getBookmark(tree::NodeID nid) const
{
    return bookmarks_.at(nid);
}

bool UserData::isBookmarked(tree::NodeID nid) const
{
    return bookmarks_.find(nid) != bookmarks_.end();
}

void UserData::clearBookmark(NodeID nid)
{
    bookmarks_.erase(nid);
}

std::vector<tree::NodeID> UserData::bookmarkedNodes() const
{

    std::vector<tree::NodeID> res;
    for (const auto &item : bookmarks_)
    {
        res.push_back(item.first);
    }

    return res;
}

} // namespace cpprofiler