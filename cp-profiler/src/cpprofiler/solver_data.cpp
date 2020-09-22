#include "solver_data.hh"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

namespace cpprofiler
{

/// convert json array to std::vector
static std::vector<int> parse_reasons_json(QJsonValue &json_arr)
{
    std::vector<int> res;

    auto arr = json_arr.toArray();

    for (auto el : arr)
    {

        if (el.isDouble())
        {
            res.push_back(el.toInt());
        }
    }

    return res;
}

/// convert json array to std::vector
static std::vector<SolverID> parse_nogoods_json(QJsonValue &json_arr)
{
    std::vector<SolverID> res;

    auto arr = json_arr.toArray();

    for (auto el : arr)
    {

        if (!el.isArray())
        {
            const auto obj = el.toObject();
            /// object should be of the form {"nid":<>, "rid":<>, "tid":<>}

            auto nid_obj = obj.value("nid");
            auto rid_obj = obj.value("rid");
            auto tid_obj = obj.value("tid");

            if (nid_obj.isDouble() && rid_obj.isDouble() && tid_obj.isDouble())
            {

                const auto nid = nid_obj.toInt();
                const auto rid = rid_obj.toInt();
                const auto tid = tid_obj.toInt();

                res.push_back({nid, rid, tid});
            }
        }
    }

    return res;
}

void SolverData::processInfo(NodeID nid, const std::string &info_str)
{
    auto info_bytes = QByteArray::fromStdString(info_str);
    QJsonParseError json_err;
    auto json_doc = QJsonDocument::fromJson(info_bytes, &json_err);

    if (json_err.error != QJsonParseError::NoError) {
        print("QJsonDocument::fromJson() error: {}\ninfo_str:\n {}\n", json_err.errorString(), info_str);
        return;
    }

    if (json_doc.isNull() || json_doc.isEmpty())
    {
        print("no info for node {}", nid);
        return;
    }

    setInfo(nid, info_str);

    QJsonObject json_obj = json_doc.object();

    if (json_obj.isEmpty())
    {
        return;
    }

    auto reasons = json_obj.value("reasons");
    if (reasons.isArray())
    {
        auto constraints = parse_reasons_json(reasons);
        // print("constraints for {}: {}", nid, constraints);
        contrib_cs_.insert({nid, std::move(constraints)});
    }

    auto nogoods_json = json_obj.value("nogoods");
    if (nogoods_json.isArray())
    {
        auto nogoods = parse_nogoods_json(nogoods_json);

        /// Nogoods contributing to the failure at `nid`
        std::vector<NodeID> c_nogoods;

        for (const auto sid : nogoods)
        {
            auto ng_nid = getNodeId(sid);

            if (ng_nid != NodeID::NoNode)
            {
                c_nogoods.push_back(ng_nid);
            }
        }

        // print("responsible nogoods for {}: {}", nid, c_nogoods);

        contrib_ngs_.insert({nid, std::move(c_nogoods)});
    }
}

void IdMap::addPair(SolverID sid, tree::NodeID nid)
{
    QWriteLocker locker(&m_lock);

    uid2id_.insert({sid, nid});
    nid2uid_.insert({nid, sid});
}

tree::NodeID IdMap::get(SolverID sid) const
{
    QReadLocker locker(&m_lock);

    const auto it = uid2id_.find(sid);

    if (it != uid2id_.end())
    {
        return it->second;
    }
    else
    {
        return NodeID::NoNode;
    }
}

} // namespace cpprofiler