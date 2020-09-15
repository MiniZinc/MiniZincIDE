#pragma once

#include <QReadWriteLock>
#include <unordered_map>

#include "core.hh"

#include "solver_id.hh"

namespace cpprofiler
{

class NameMap;

class IdMap
{

    mutable QReadWriteLock m_lock;

    std::unordered_map<SolverID, NodeID> uid2id_;

    std::unordered_map<NodeID, SolverID> nid2uid_;

  public:
    void addPair(SolverID, NodeID);

    NodeID get(SolverID) const;

    SolverID getUID(NodeID nid) const
    {
        auto it = nid2uid_.find(nid);

        if (it != nid2uid_.end())
        {
            return nid2uid_.at(nid);
        }
        else
        {
            return {-1, -1, -1};
        }
    }
};

class SolverData
{

    /// TODO:save/load id map to/from DB
    IdMap m_id_map;

    std::unordered_map<NodeID, Info> info_map_;

    std::unordered_map<NodeID, Nogood> nogood_map_;

    /// Constraints contributing to a no-good at NodeID
    std::unordered_map<NodeID, std::vector<int>> contrib_cs_;

    /// Nogoods contributing to the failure at node NodeID
    std::unordered_map<NodeID, std::vector<NodeID>> contrib_ngs_;

    /// Time since the beginning of solving process;
    std::unordered_map<NodeID, int> node_time_;

  public:
    NodeID getNodeId(SolverID sid) const
    {
        return m_id_map.get(sid);
    }

    void setNodeId(SolverID sid, NodeID nid)
    {
        m_id_map.addPair(sid, nid);
    }

    SolverID getSolverID(NodeID nid) const
    {
        return m_id_map.getUID(nid);
    }

    /// Get the reasons (constraint ids) for the nogood at node `nid`
    const std::vector<int> *getContribConstraints(NodeID nid) const
    {
        const auto it = contrib_cs_.find(nid);

        if (it == contrib_cs_.end())
            return nullptr;
        return &(it->second);
    }

    /// Get nogoods (identified by the NodeID where they were created)
    /// that contribute to the failure at `nid`;
    const std::vector<NodeID> *getContribNogoods(NodeID nid) const
    {
        const auto it = contrib_ngs_.find(nid);

        if (it == contrib_ngs_.end())
            return nullptr;
        return &(it->second);
    }

    /// Associate nogood `ng` with node `nid`
    void setNogood(NodeID nid, const std::string &orig, const std::string &renamed)
    {
        nogood_map_.insert({nid, Nogood(orig, renamed)});
    }

    void setNogood(NodeID nid, const std::string &orig)
    {
        nogood_map_.insert({nid, Nogood(orig)});
    }

    const Nogood &getNogood(NodeID nid) const
    {
        auto it = nogood_map_.find(nid);
        if (it != nogood_map_.end())
        {
            return it->second;
        }
        else
        {
            return Nogood::empty;
        }
    }

    void setInfo(NodeID nid, const std::string &orig)
    {
        info_map_.insert({nid, Info(orig)});
    }

    const Info &getInfo(NodeID nid) const
    {
        auto it = info_map_.find(nid);
        if (it != info_map_.end())
        {
            return it->second;
        }
        else
        {
            return Info("");
        }
    }

    /// Process node info looking for reasons, contributing nogoods for failed nodes etc.
    void processInfo(NodeID nid, const std::string &info_str);

    /// Whether the data stores at least one no-good
    bool hasNogoods() const
    {
        return !nogood_map_.empty();
    }

    /// Whether the data stores at least one no-good
    bool hasInfo() const
    {
        return !info_map_.empty();
    }
};

} // namespace cpprofiler
