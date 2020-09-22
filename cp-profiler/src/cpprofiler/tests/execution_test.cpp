#include "execution_test.hh"

#include "../core.hh"
#include "../conductor.hh"
#include "../user_data.hh"

#include "../execution.hh"
#include "../tree/structure.hh"
#include "../utils/array.hh"
#include "../utils/utils.hh"
#include "../tree/node_info.hh"
#include "../execution_window.hh"
#include "../tree/traditional_view.hh"

#include "../db_handler.hh"

#include <QDebug>
#include <iostream>

namespace cpprofiler
{
namespace tests
{
namespace execution
{

void copy_test(utils::Array<int> arr)
{
    auto new_arr = arr;
}

void array_test()
{

    utils::Array<int> arr(1);

    copy_test(arr);
}

void binary_tree_execution(Conductor &conductor)
{

    auto ex = conductor.addNewExecution("test execution");

    auto &tree = ex->tree();

    auto root = tree.createRoot(2, "0");
    auto n1 = tree.promoteNode(root, 0, 2, tree::NodeStatus::BRANCH, "1");
    auto n2 = tree.promoteNode(root, 1, 2, tree::NodeStatus::BRANCH, "2");

    ex->userData().setSelectedNode(n1);

    auto n3 = tree.promoteNode(n1, 0, 2, tree::NodeStatus::BRANCH, "3");
    auto n4 = tree.promoteNode(n1, 1, 2, tree::NodeStatus::BRANCH, "4");

    auto n5 = tree.promoteNode(n3, 0, 2, tree::NodeStatus::BRANCH, "5");
    auto n6 = tree.promoteNode(n3, 1, 2, tree::NodeStatus::BRANCH, "6");

    tree.promoteNode(n5, 0, 0, tree::NodeStatus::SOLVED, "7");
    tree.promoteNode(n5, 1, 0, tree::NodeStatus::FAILED, "8");

    tree.promoteNode(n6, 0, 0, tree::NodeStatus::FAILED, "9");
    tree.promoteNode(n6, 1, 0, tree::NodeStatus::SKIPPED, "10");

    auto n7 = tree.promoteNode(n4, 0, 0, tree::NodeStatus::FAILED, "11");
    auto n8 = tree.promoteNode(n4, 1, 0, tree::NodeStatus::FAILED, "12");

    auto n9 = tree.promoteNode(n2, 0, 2, tree::NodeStatus::BRANCH, "13");
    auto n10 = tree.promoteNode(n2, 1, 0, tree::NodeStatus::FAILED, "14");

    auto n11 = tree.promoteNode(n9, 0, 2, tree::NodeStatus::BRANCH, "15");
    auto n12 = tree.promoteNode(n9, 1, 0, tree::NodeStatus::FAILED, "16");

    auto n13 = tree.promoteNode(n11, 0, 2, tree::NodeStatus::BRANCH, "17");
    auto n14 = tree.promoteNode(n11, 1, 0, tree::NodeStatus::FAILED, "18");

    auto n15 = tree.promoteNode(n13, 0, 2, tree::NodeStatus::BRANCH, "19");
    auto n16 = tree.promoteNode(n13, 1, 0, tree::NodeStatus::FAILED, "20");

    auto n17 = tree.promoteNode(n15, 0, 2, tree::NodeStatus::BRANCH, "21");
    auto n18 = tree.promoteNode(n15, 1, 0, tree::NodeStatus::FAILED, "22");

    auto n19 = tree.promoteNode(n17, 0, 2, tree::NodeStatus::BRANCH, "23");
    auto n20 = tree.promoteNode(n17, 1, 0, tree::NodeStatus::FAILED, "24");

    auto n21 = tree.promoteNode(n19, 0, 2, tree::NodeStatus::BRANCH, "25");
    auto n22 = tree.promoteNode(n19, 1, 0, tree::NodeStatus::FAILED, "26");

    auto n23 = tree.promoteNode(n21, 0, 2, tree::NodeStatus::BRANCH, "27");
    auto n24 = tree.promoteNode(n21, 1, 0, tree::NodeStatus::UNDETERMINED, "28");

    auto n25 = tree.promoteNode(n23, 0, 2, tree::NodeStatus::BRANCH, "29");
    auto n26 = tree.promoteNode(n23, 1, 0, tree::NodeStatus::FAILED, "30");

    auto n27 = tree.promoteNode(n25, 0, 2, tree::NodeStatus::BRANCH, "31");
    auto n28 = tree.promoteNode(n25, 1, 0, tree::NodeStatus::FAILED, "32");

    auto n29 = tree.promoteNode(n27, 0, 2, tree::NodeStatus::BRANCH, "33");
    auto n30 = tree.promoteNode(n27, 1, 0, tree::NodeStatus::FAILED, "34");

    auto n31 = tree.promoteNode(n29, 0, 2, tree::NodeStatus::BRANCH, "35");
    auto n32 = tree.promoteNode(n29, 1, 0, tree::NodeStatus::FAILED, "36");

    auto n33 = tree.promoteNode(n31, 0, 2, tree::NodeStatus::BRANCH, "37");
    auto n34 = tree.promoteNode(n31, 1, 0, tree::NodeStatus::FAILED, "38");

    auto n35 = tree.promoteNode(n33, 0, 0, tree::NodeStatus::FAILED, "39");
    auto n36 = tree.promoteNode(n33, 1, 0, tree::NodeStatus::FAILED, "40");

    conductor.showTraditionalView(ex);
}

void nary_execution(Conductor &conductor)
{

    auto ex = conductor.addNewExecution("n-ary execution");

    auto &tree = ex->tree();

    auto root = tree.createRoot(4);
    auto n1 = tree.promoteNode(root, 0, 2, tree::NodeStatus::BRANCH, "1");
    auto n2 = tree.promoteNode(root, 1, 2, tree::NodeStatus::BRANCH, "2");
    auto n3 = tree.promoteNode(root, 2, 0, tree::NodeStatus::FAILED, "3");
    auto n4 = tree.promoteNode(root, 3, 2, tree::NodeStatus::BRANCH, "4");

    auto n5 = tree.promoteNode(n1, 0, 0, tree::NodeStatus::FAILED, "5");
    auto n6 = tree.promoteNode(n1, 1, 0, tree::NodeStatus::FAILED, "6");

    auto n7 = tree.promoteNode(n2, 0, 0, tree::NodeStatus::FAILED, "7");
    auto n8 = tree.promoteNode(n2, 1, 2, tree::NodeStatus::BRANCH, "8");

    auto n9 = tree.promoteNode(n4, 0, 0, tree::NodeStatus::FAILED, "9");
    auto n10 = tree.promoteNode(n4, 1, 0, tree::NodeStatus::FAILED, "10");

    auto n11 = tree.promoteNode(n8, 0, 0, tree::NodeStatus::FAILED, "11");
    auto n12 = tree.promoteNode(n8, 1, 0, tree::NodeStatus::FAILED, "12");
}

void larger_nary_execution(Conductor &conductor)
{

    auto ex = conductor.addNewExecution("n-ary execution");

    auto &tree = ex->tree();

    auto root = tree.createRoot(4);
    auto n1 = tree.promoteNode(root, 0, 2, tree::NodeStatus::BRANCH);
    auto n2 = tree.promoteNode(root, 1, 2, tree::NodeStatus::BRANCH);
    auto n3 = tree.promoteNode(root, 2, 0, tree::NodeStatus::FAILED);
    auto n4 = tree.promoteNode(root, 3, 2, tree::NodeStatus::BRANCH);

    auto n5 = tree.promoteNode(n1, 0, 0, tree::NodeStatus::FAILED);
    auto n6 = tree.promoteNode(n1, 1, 12, tree::NodeStatus::BRANCH);

    auto n6a = tree.promoteNode(n6, 0, 0, tree::NodeStatus::FAILED);
    auto n6b = tree.promoteNode(n6, 1, 0, tree::NodeStatus::FAILED);
    auto n6c = tree.promoteNode(n6, 2, 0, tree::NodeStatus::FAILED);
    auto n6d = tree.promoteNode(n6, 3, 0, tree::NodeStatus::FAILED);
    tree.promoteNode(n6, 4, 0, tree::NodeStatus::FAILED);
    tree.promoteNode(n6, 5, 0, tree::NodeStatus::FAILED);
    tree.promoteNode(n6, 6, 0, tree::NodeStatus::FAILED);
    tree.promoteNode(n6, 7, 0, tree::NodeStatus::FAILED);
    tree.promoteNode(n6, 8, 0, tree::NodeStatus::FAILED);
    tree.promoteNode(n6, 9, 0, tree::NodeStatus::FAILED);
    tree.promoteNode(n6, 10, 0, tree::NodeStatus::FAILED);
    tree.promoteNode(n6, 11, 0, tree::NodeStatus::FAILED);

    auto n7 = tree.promoteNode(n2, 0, 0, tree::NodeStatus::FAILED);
    auto n8 = tree.promoteNode(n2, 1, 2, tree::NodeStatus::BRANCH);

    auto n9 = tree.promoteNode(n4, 0, 0, tree::NodeStatus::FAILED);
    auto n10 = tree.promoteNode(n4, 1, 0, tree::NodeStatus::FAILED);

    auto n11 = tree.promoteNode(n8, 0, 0, tree::NodeStatus::FAILED);
    auto n12 = tree.promoteNode(n8, 1, 0, tree::NodeStatus::FAILED);
}

void simple_nary_execution(Conductor &conductor)
{

    auto ex = conductor.addNewExecution("n-ary execution");

    auto &tree = ex->tree();

    auto root = tree.createRoot(4);
    auto n1 = tree.promoteNode(root, 0, 0, tree::NodeStatus::FAILED);
    auto n2 = tree.promoteNode(root, 1, 0, tree::NodeStatus::FAILED);
    auto n3 = tree.promoteNode(root, 2, 0, tree::NodeStatus::FAILED);
    auto n4 = tree.promoteNode(root, 3, 0, tree::NodeStatus::FAILED);
}

void binary_test_1_for_identical_subtrees(Conductor &conductor)
{

    auto ex = conductor.addNewExecution("test for identical subtree algorithm");

    auto &tree = ex->tree();

    auto root = tree.createRoot(2);
    auto n1 = tree.promoteNode(root, 0, 2, tree::NodeStatus::BRANCH, "a");
    auto n2 = tree.promoteNode(root, 1, 2, tree::NodeStatus::BRANCH, "b");

    auto n3 = tree.promoteNode(n1, 0, 2, tree::NodeStatus::BRANCH, "c");
    auto n4 = tree.promoteNode(n1, 1, 2, tree::NodeStatus::BRANCH, "d");
    auto n11 = tree.promoteNode(n2, 0, 0, tree::NodeStatus::FAILED, "e");
    auto n12 = tree.promoteNode(n2, 1, 2, tree::NodeStatus::BRANCH, "f");

    auto n5 = tree.promoteNode(n3, 0, 0, tree::NodeStatus::FAILED, "g");
    auto n6 = tree.promoteNode(n3, 1, 2, tree::NodeStatus::BRANCH, "h");
    auto n9 = tree.promoteNode(n4, 0, 0, tree::NodeStatus::FAILED, "i");
    auto n10 = tree.promoteNode(n4, 1, 0, tree::NodeStatus::FAILED, "j");
    auto n13 = tree.promoteNode(n12, 0, 0, tree::NodeStatus::FAILED, "k");
    auto n14 = tree.promoteNode(n12, 1, 0, tree::NodeStatus::FAILED, "l");

    auto n7 = tree.promoteNode(n6, 0, 0, tree::NodeStatus::FAILED, "m");
    auto n8 = tree.promoteNode(n6, 1, 0, tree::NodeStatus::FAILED, "n");
}

void binary_test_2_for_identical_subtrees(Conductor &conductor)
{

    auto ex = conductor.addNewExecution("test for identical subtree algorithm");

    auto &tree = ex->tree();

    auto root = tree.createRoot(2, "0");
    auto n1 = tree.promoteNode(root, 0, 2, tree::NodeStatus::BRANCH, "1");
    auto n2 = tree.promoteNode(root, 1, 2, tree::NodeStatus::BRANCH, "2");

    auto n3 = tree.promoteNode(n1, 0, 2, tree::NodeStatus::BRANCH, "3");
    auto n4 = tree.promoteNode(n1, 1, 2, tree::NodeStatus::BRANCH, "4");
    auto n5 = tree.promoteNode(n2, 0, 2, tree::NodeStatus::BRANCH, "5");
    auto n6 = tree.promoteNode(n2, 1, 2, tree::NodeStatus::BRANCH, "6");

    auto n7 = tree.promoteNode(n3, 0, 0, tree::NodeStatus::FAILED, "7");
    auto n8 = tree.promoteNode(n3, 1, 0, tree::NodeStatus::FAILED, "8");
    auto n9 = tree.promoteNode(n4, 0, 0, tree::NodeStatus::FAILED, "9");
    auto n10 = tree.promoteNode(n4, 1, 2, tree::NodeStatus::BRANCH, "10");
    auto n13 = tree.promoteNode(n6, 0, 2, tree::NodeStatus::BRANCH, "13");
    auto n14 = tree.promoteNode(n6, 1, 2, tree::NodeStatus::BRANCH, "14");

    auto n15 = tree.promoteNode(n10, 0, 0, tree::NodeStatus::FAILED, "15");
    auto n16 = tree.promoteNode(n10, 1, 0, tree::NodeStatus::FAILED, "16");

    auto n11 = tree.promoteNode(n5, 0, 0, tree::NodeStatus::FAILED, "11");
    auto n12 = tree.promoteNode(n5, 1, 0, tree::NodeStatus::FAILED, "12");

    auto n17 = tree.promoteNode(n13, 0, 0, tree::NodeStatus::FAILED, "17");
    auto n18 = tree.promoteNode(n13, 1, 0, tree::NodeStatus::FAILED, "18");

    auto n19 = tree.promoteNode(n14, 0, 0, tree::NodeStatus::FAILED, "19");
    auto n20 = tree.promoteNode(n14, 1, 2, tree::NodeStatus::BRANCH, "20");

    auto n21 = tree.promoteNode(n20, 0, 0, tree::NodeStatus::FAILED, "21");
    auto n22 = tree.promoteNode(n20, 1, 0, tree::NodeStatus::FAILED, "22");
}

void build_for_comparison_a(tree::NodeTree &tree)
{

    auto root = tree.createRoot(2, "0");

    auto n1 = tree.promoteNode(root, 0, 2, tree::NodeStatus::BRANCH, "1");
    auto n2 = tree.promoteNode(root, 1, 2, tree::NodeStatus::BRANCH, "2");

    auto n3 = tree.promoteNode(n1, 0, 0, tree::NodeStatus::FAILED, "3");
    auto n4 = tree.promoteNode(n1, 1, 2, tree::NodeStatus::BRANCH, "4");

    auto n7 = tree.promoteNode(n4, 0, 0, tree::NodeStatus::FAILED, "7");
    auto n8 = tree.promoteNode(n4, 1, 0, tree::NodeStatus::FAILED, "8");

    auto n5 = tree.promoteNode(n2, 0, 0, tree::NodeStatus::SOLVED, "5");
    auto n6 = tree.promoteNode(n2, 1, 0, tree::NodeStatus::FAILED, "6");
}

void build_for_comparison_b(tree::NodeTree &tree)
{

    auto root = tree.createRoot(2, "0");

    auto n1 = tree.promoteNode(root, 0, 2, tree::NodeStatus::BRANCH, "1");
    auto n2 = tree.promoteNode(root, 1, 0, tree::NodeStatus::FAILED, "2");

    auto n3 = tree.promoteNode(n1, 0, 0, tree::NodeStatus::FAILED, "3");
    auto n4 = tree.promoteNode(n1, 1, 2, tree::NodeStatus::BRANCH, "4");

    auto n7 = tree.promoteNode(n4, 0, 2, tree::NodeStatus::BRANCH, "7");
    auto n8 = tree.promoteNode(n4, 1, 0, tree::NodeStatus::FAILED, "8");

    auto n9 = tree.promoteNode(n7, 0, 0, tree::NodeStatus::FAILED, "9");
    auto n10 = tree.promoteNode(n7, 1, 0, tree::NodeStatus::FAILED, "10");
}

void comparison(Conductor &c)
{

    auto ex1 = c.addNewExecution("Execution A");
    build_for_comparison_a(ex1->tree());

    auto ex2 = c.addNewExecution("Execution B");
    build_for_comparison_b(ex2->tree());

    c.mergeTrees(ex1, ex2);
}

void comparison2(Conductor &c)
{

    const auto path = "/home/maxim/dev/cp-profiler2/golomb8.db";

    auto ex1 = db_handler::load_execution(path);
    if (ex1)
        c.addNewExecution(ex1);

    auto ex2 = db_handler::load_execution(path);
    if (ex2)
        c.addNewExecution(ex2);

    c.mergeTrees(ex1.get(), ex2.get());
}

void tree_building(Conductor &c)
{

    /// create a dummy root node
    auto ex = c.addNewExecution("test tree");

    auto &tree = ex->tree();

    auto root = tree.createRoot(0);

    auto n1 = tree.promoteNode(NodeID::NoNode, -1, 0, tree::NodeStatus::FAILED, "1");
    //     auto n2 = tree.promoteNode(root, 1, 0, tree::NodeStatus::FAILED, "2");

    //     auto n3 = tree.promoteNode(n1, 0, 0, tree::NodeStatus::FAILED, "3");
    //     auto n4 = tree.promoteNode(n1, 1, 0, tree::NodeStatus::FAILED, "4");
}

void hiding_failed_test(Conductor &c)
{

    auto ex = c.addNewExecution("test hiding failed");

    auto &tree = ex->tree();

    auto root = tree.createRoot(2, "0");

    auto n1 = tree.promoteNode(root, 0, 2, tree::NodeStatus::BRANCH, "1");
    auto n2 = tree.promoteNode(root, 1, 2, tree::NodeStatus::BRANCH, "2");

    auto n3 = tree.promoteNode(n1, 0, 2, tree::NodeStatus::BRANCH, "3");
    auto n4 = tree.promoteNode(n1, 1, 0, tree::NodeStatus::SOLVED, "4");

    auto n5 = tree.promoteNode(n3, 0, 0, tree::NodeStatus::FAILED, "5");
    auto n6 = tree.promoteNode(n3, 1, 0, tree::NodeStatus::FAILED, "6");

    auto n7 = tree.promoteNode(n2, 0, 0, tree::NodeStatus::UNDETERMINED, "7");
    auto n8 = tree.promoteNode(n2, 1, 2, tree::NodeStatus::BRANCH, "8");

    auto n9 = tree.promoteNode(n8, 0, 0, tree::NodeStatus::FAILED, "9");
    auto n10 = tree.promoteNode(n8, 1, 0, tree::NodeStatus::FAILED, "10");
}

void restart_tree(Conductor &c)
{

    auto ex = c.addNewExecution("Restart Tree");

    auto &tree = ex->tree();

    utils::MutexLocker locker(&tree.treeMutex());

    auto root = tree.createRoot(0);

    tree.addExtraChild(root);
    // tree.addExtraChild(root);
    // tree.addExtraChild(root);
    // tree.addExtraChild(root);
    // tree.addExtraChild(root);
    // tree.addExtraChild(root);

    // tree.addExtraChild(tree.getChild(root, 3));
}

static void load_execution(Conductor &c, const char *path)
{

    auto ex = db_handler::load_execution(path);

    if (!ex)
    {
        print("could not load the execution");
    }
    else
    {
        c.addNewExecution(ex);
    }
}

static void save_and_load(Conductor &c, const char *path)
{

    auto ex1 = c.addNewExecution("simple execution");
    build_for_comparison_a(ex1->tree());

    ex1->userData().setBookmark(NodeID{2}, "Test Bookmark");

    db_handler::save_execution(ex1, path);

    auto ex = db_handler::load_execution(path);
    if (!ex)
    {
        print("could not load the execution");
    }
    else
    {
        c.addNewExecution(ex);
    }
}

static void db_create_tree(Conductor &c)
{

    auto ex = c.addNewExecution("Created as in DB");
    auto &tree = ex->tree();

    tree.db_initialize(100);

    tree.db_createRoot(NodeID{0});

    tree.db_addChild(NodeID{1}, NodeID{0}, 0, tree::NodeStatus::BRANCH, "a");
    tree.db_addChild(NodeID{2}, NodeID{0}, 1, tree::NodeStatus::BRANCH, "b");
}

static void save_search(Conductor &c)
{

    auto ex1 = c.addNewExecution("simple execution");
    build_for_comparison_a(ex1->tree());

    c.saveSearch(ex1, "test.search");
}

/// similar subtree
static void ss_analysis(Conductor &c)
{

    const char *path = "/home/maxim/dev/cp-profiler2/golomb6.db";
    // const char *path = "/home/maxim/dev/cp-profiler2/golomb10.db";
    auto ex = db_handler::load_execution(path);
    if (!ex)
    {
        print("could not load the execution");
    }
    else
    {
        c.addNewExecution(ex);
        c.showTraditionalView(ex.get());
    }
}

static void nogood_dialog(Conductor &c)
{

    const char *orig_path = "/home/maxim/dev/cp-profiler2/nogoods.db";

    auto ex = db_handler::load_execution(orig_path);
    if (!ex)
    {
        print("could not load the execution");
    }
    else
    {
        c.addNewExecution(ex);
        // c.showTraditionalView(ex.get());
    }

    const char *replayed_path = "/home/maxim/dev/cp-profiler2/nogoods_replayed.db";

    auto ex2 = db_handler::load_execution(replayed_path);
    if (!ex2)
    {
        print("could not load the execution");
    }
    else
    {
        c.addNewExecution(ex2);
        // c.showTraditionalView(ex2.get());
    }

    c.runNogoodAnalysis(ex.get(), ex2.get());

    // c.mergeTrees(ex.get(), ex2.get());
}

void run(Conductor &c)
{

    /// this one works with db
    // binary_test_1_for_identical_subtrees(c);

    // binary_test_2_for_identical_subtrees(c);

    // binary_tree_execution(c);
    // simple_nary_execution(c);
    // nary_execution(c);
    // larger_nary_execution(c);

    // hiding_failed_test(c);

    // comparison(c);

    // comparison2(c);

    // tree_building(c);

    // restart_tree(c);

    // load_execution(c, "/home/maxim/dev/cp-profiler2/golomb8.db");

    // save_and_load(c, "/home/maxim/dev/cp-profiler2/test.db");

    // nogood_dialog(c);

    // db_create_tree(c);

    // save_search(c);

    // ss_analysis(c);
}

} // namespace execution
} // namespace tests
} // namespace cpprofiler
