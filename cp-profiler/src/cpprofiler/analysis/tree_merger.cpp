#include "tree_merger.hh"
#include "../execution.hh"
#include "../tree/structure.hh"
#include "../core.hh"

#include "../utils/utils.hh"
#include "../utils/tree_utils.hh"

#include <QStack>

namespace cpprofiler
{
namespace analysis
{

using namespace tree;

TreeMerger::TreeMerger(const Execution &ex_l_,
                       const Execution &ex_r_,
                       std::shared_ptr<tree::NodeTree> tree,
                       std::shared_ptr<analysis::MergeResult> res,
                       std::shared_ptr<std::vector<OriginalLoc>> orig_locs)
    : ex_l(ex_l_), ex_r(ex_r_),
      tree_l(ex_l.tree()),
      tree_r(ex_r.tree()),
      res_tree(tree),
      merge_result(res),
      orig_locs_(orig_locs)
{

    connect(this, &QThread::finished, this, &QObject::deleteLater);
}

TreeMerger::~TreeMerger()
{
}

static void find_and_replace_all(std::string &str, std::string substr_old, std::string substr_new)
{
    auto pos = str.find(substr_old);
    while (pos != std::string::npos)
    {
        str.replace(pos, std::string(substr_old).length(), substr_new);
        pos = str.find(substr_old);
    }
}

static bool labelsEqual(std::string lhs, std::string rhs)
{
    /// NOTE(maxim): removes whitespaces before comparing;
    /// this will be necessary as long as Chuffed and Gecode don't agree
    /// on whether to put whitespaces around operators (Gecode uses ' '
    /// for parsing logbrancher while Chuffed uses them as a delimiter
    /// between literals)

    if (lhs.substr(0, 3) == "[i]" || lhs.substr(0, 3) == "[f]")
    {
        lhs = lhs.substr(3);
    }

    if (rhs.substr(0, 3) == "[i]" || rhs.substr(0, 3) == "[f]")
    {
        rhs = rhs.substr(3);
    }

    lhs.erase(remove_if(lhs.begin(), lhs.end(), isspace), lhs.end());
    rhs.erase(remove_if(rhs.begin(), rhs.end(), isspace), rhs.end());

    find_and_replace_all(lhs, "==", "=");
    find_and_replace_all(rhs, "==", "=");

    // qDebug() << "comparing: " << lhs.c_str() << " " << rhs.c_str();

    if (lhs.compare(rhs) != 0)
    {
        return false;
    }

    return true;
}

static bool compareNodes(NodeID n1, const NodeTree &nt1,
                         NodeID n2, const NodeTree &nt2,
                         bool with_labels)
{

    if (n1 == NodeID::NoNode || n2 == NodeID::NoNode)
        return false;

    if (nt1.getStatus(n1) != nt2.getStatus(n2))
        return false;

    if (with_labels)
    {
        const auto &label1 = nt1.getLabel(n1);
        const auto &label2 = nt2.getLabel(n2);
        if (!labelsEqual(label1, label2))
            return false;
    }

    return true;
}

/// Copy the subtree rooted at nid_s of nt_s as a subtree rooted at nid in nt
static void copy_tree_into(NodeTree &nt, NodeID nid, const NodeTree &nt_s, NodeID nid_s)
{

    QStack<NodeID> stack;
    QStack<NodeID> stack_s;
    stack.push(nid);
    stack_s.push(nid_s);

    while (stack_s.size() > 0)
    {
        auto node_s = stack_s.pop();
        auto node = stack.pop();

        auto kids = nt_s.childrenCount(node_s);
        auto status = nt_s.getStatus(node_s);

        /// TODO: make sure only the reference to a label is stored
        auto label = nt_s.getLabel(node_s);

        nt.promoteNode(node, kids, status, label);

        for (auto alt = 0; alt < kids; ++alt)
        {
            stack.push(nt.getChild(node, alt));
            stack_s.push(nt_s.getChild(node_s, alt));
        }
    }
}

void create_pentagon(NodeTree &nt, NodeID nid,
                     const NodeTree &nt_l, NodeID nid_l,
                     const NodeTree &nt_r, NodeID nid_r)
{

    nt.promoteNode(nid, 2, NodeStatus::MERGED);

    /// copy the subtree of nt_l into target_l
    if (nid_l != NodeID::NoNode)
    {
        auto target_l = nt.getChild(nid, 0);
        copy_tree_into(nt, target_l, nt_l, nid_l);
    }

    /// copy the subtree of nt_r into target_r
    if (nid_r != NodeID::NoNode)
    {
        auto target_r = nt.getChild(nid, 1);
        copy_tree_into(nt, target_r, nt_r, nid_r);
    }
}

void TreeMerger::run()
{

    print("Merging: running...");

    /// It is quite unlikely, but this can dead-lock (needs consistent ordering)
    utils::MutexLocker locker_l(&tree_l.treeMutex());
    utils::MutexLocker locker_r(&tree_r.treeMutex());
    utils::MutexLocker locker_res(&res_tree->treeMutex());

    QStack<NodeID> stack_l, stack_r, stack;

    auto root_l = tree_l.getRoot();
    auto root_r = tree_r.getRoot();

    stack_l.push(root_l);
    stack_r.push(root_r);

    auto root = res_tree->createRoot(0);

    stack.push(root);

    while (stack_l.size() > 0)
    {

        auto node_l = stack_l.pop();
        auto node_r = stack_r.pop();
        auto target = stack.pop();

        bool equal = compareNodes(node_l, tree_l, node_r, tree_r, false);

        if (equal)
        {

            const auto kids_l = tree_l.childrenCount(node_l);
            const auto kids_r = tree_r.childrenCount(node_r);

            const auto min_kids = std::min(kids_l, kids_r);
            const auto max_kids = std::max(kids_l, kids_r);

            { /// The merged tree will always have the number of children of the 'larger' tree
                auto status = tree_l.getStatus(node_l);
                auto label = tree_l.getLabel(node_l);
                res_tree->promoteNode(target, max_kids, status, label);
            }

            if (min_kids == max_kids)
            {
                /// ----- MERGE COMPLETELY -----
                for (auto i = max_kids - 1; i >= 0; --i)
                {
                    stack_l.push(tree_l.getChild(node_l, i));
                    stack_r.push(tree_r.getChild(node_r, i));
                    stack.push(res_tree->getChild(target, i));
                }
            }
            else
            {
                /// ----- MERGE PARTIALLY -----

                /// For every "extra" child
                for (auto i = max_kids - 1; i >= min_kids; --i)
                {

                    if (kids_l > kids_r)
                    {
                        const auto kid_l = tree_l.getChild(node_l, i);
                        const auto status = tree_l.getStatus(kid_l);

                        /// NOTE(maxim): this is most likely the case of replaying with skipped nodes,
                        /// so should not be compared (the same below)
                        if (status == NodeStatus::UNDETERMINED || status == NodeStatus::SKIPPED)
                        {
                            continue;
                        }

                        stack_l.push(kid_l);
                        stack_r.push(NodeID::NoNode);
                        stack.push(res_tree->getChild(target, i));
                    }
                    else
                    {
                        const auto kid_r = tree_r.getChild(node_r, i);
                        const auto status = tree_r.getStatus(kid_r);

                        if (status == NodeStatus::UNDETERMINED || status == NodeStatus::SKIPPED)
                        {
                            continue;
                        }

                        stack_l.push(NodeID::NoNode);
                        stack_r.push(kid_r);
                        stack.push(res_tree->getChild(target, i));
                    }
                }

                /// For every child in common
                for (auto i = min_kids - 1; i >= 0; --i)
                {
                    stack_l.push(tree_l.getChild(node_l, i));
                    stack_r.push(tree_r.getChild(node_r, i));
                    stack.push(res_tree->getChild(target, i));
                }
            }
        }
        else
        {
            create_pentagon(*res_tree, target, tree_l, node_l, tree_r, node_r);

            auto count_left = utils::count_descendants(tree_l, node_l);
            auto count_right = utils::count_descendants(tree_r, node_r);
            auto pen_item = PentagonItem{target, count_left, count_right};

            merge_result->push_back(pen_item);
        }
    }

    print("Merging: done");
}

} // namespace analysis
} // namespace cpprofiler