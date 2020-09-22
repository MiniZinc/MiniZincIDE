#ifndef CPPROFILER_USER_DATA_HH
#define CPPROFILER_USER_DATA_HH

#include "tree/node_id.hh"
#include "core.hh"
#include <unordered_map>
#include <string>
#include <vector>

namespace cpprofiler
{

/// TODO: move this to Execution
class UserData
{

    tree::NodeID m_selected_node = tree::NodeID::NoNode;

    std::unordered_map<NodeID, std::string> bookmarks_;

  public:
    void setSelectedNode(tree::NodeID nid);

    /// returns currently selected node (possibly NodeID::NoNode)
    tree::NodeID getSelectedNode() const;

    void setBookmark(tree::NodeID nid, const std::string &text);

    void clearBookmark(NodeID nid);

    const std::string &getBookmark(tree::NodeID nid) const;

    bool isBookmarked(tree::NodeID nid) const;

    std::vector<tree::NodeID> bookmarkedNodes() const;
};

} // namespace cpprofiler

#endif