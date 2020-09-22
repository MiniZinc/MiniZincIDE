#include "db_handler.hh"

#include <fstream>
#include "utils/debug.hh"
#include <QSqlDatabase>
#include <QSqlQuery>
#include "execution.hh"
#include "utils/tree_utils.hh"
#include "utils/perf_helper.hh"

namespace cpprofiler
{
namespace db_handler
{

typedef int (*SQL_Callback)(void *, int, char **, char **);

struct NodeData
{
    NodeID nid;
    NodeID pid;
    int alt;
    int kids;
    tree::NodeStatus status;
    std::string label;
};

struct BookmarkItem
{
    NodeID nid;
    std::string text;
};

struct NogoodItem
{
    NodeID nid;
    std::string text;
};

struct InfoItem
{
    NodeID nid;
    std::string text;
};

static int count_nodes(QSqlDatabase *db)
{
    const auto query = "select count(*) from Nodes;";
    QSqlQuery count_stmt(*db);
    count_stmt.prepare(query);
    count_stmt.exec();
    count_stmt.next();
    return count_stmt.value(0).toInt();
}

/// Reads all nodes from a database and builds the tree; returns `true` on success
static bool read_nodes(QSqlDatabase *db, Execution &ex)
{
    const auto query = "select * from Nodes;";

    QSqlQuery select_stmt (*db);
    select_stmt.prepare(query);

    auto &tree = ex.tree();

    const auto total_nodes = count_nodes(db);
    print("We have {} nodes", total_nodes);

    tree.db_initialize(total_nodes);

    bool success = select_stmt.exec();

    if(!success) return false;

    while (select_stmt.next())
    {
        const auto nid = NodeID(select_stmt.value(0).toInt());
        const auto pid = NodeID(select_stmt.value(1).toInt());
        const auto alt = select_stmt.value(2).toInt();
        const auto kids = select_stmt.value(3).toInt();
        const auto status = tree::NodeStatus(select_stmt.value(4).toInt());
        const auto label = select_stmt.value(5).toString().toStdString();

        if (pid == NodeID::NoNode)
        {
            tree.db_createRoot(nid, label);
        }
        else
        {
            tree.db_addChild(nid, pid, alt, status, label);
        }
    }

    return success;
}

/// Read all bookmarks from the database
static bool read_bookmarks(QSqlDatabase *db, Execution &ex)
{
    const auto query = "select * from Bookmarks;";
    QSqlQuery select_bm_(*db);
    select_bm_.prepare(query);

    bool success = select_bm_.exec();
    if(!success) return false;

    auto &ud = ex.userData();

    while (select_bm_.next())
    {
        const auto nid = NodeID(select_bm_.value(0).toInt());
        const auto bm_text = select_bm_.value(1).toString().toStdString();

        ud.setBookmark(nid, bm_text);
    }

    return success;
}

/// Read all Info from the database
static bool read_info(QSqlDatabase *db, Execution &ex)
{
    const auto query = "select * from Info;";
    QSqlQuery select_info_ (*db);
    select_info_.prepare(query);

    bool success = select_info_.exec();
    if(!success) return false;

    auto &sd = ex.solver_data();

    while (select_info_.next())
    {
        const auto nid = NodeID(select_info_.value(0).toInt());
        const auto info_text = select_info_.value(1).toString().toStdString();

        sd.setInfo(nid, {info_text});
    }

    return success;
}

/// Read all nogoods from the database
static bool read_nogoods(QSqlDatabase *db, Execution &ex)
{
    const auto query = "select * from Nogoods;";
    QSqlQuery select_ng_(*db);
    select_ng_.prepare(query);

    bool success = select_ng_.exec();

    auto &sd = ex.solver_data();

    while (select_ng_.next())
    {
        const auto nid = NodeID(select_ng_.value(0).toInt());
        const auto ng_text = select_ng_.value(1).toString().toStdString();

        sd.setNogood(nid, {ng_text});
    }

    return success;
}

static void insert_node(QSqlQuery *stmt, NodeData nd)
{
    stmt->finish();

    stmt->addBindValue(static_cast<int>(nd.nid));
    stmt->addBindValue(static_cast<int>(nd.pid));
    stmt->addBindValue(static_cast<int>(nd.alt));
    stmt->addBindValue(static_cast<int>(nd.kids));
    stmt->addBindValue(static_cast<int>(nd.status));
    stmt->addBindValue(nd.label.c_str());

    if (!stmt->exec())
    {
        print("ERROR: could not execute db statement");
    }
}

static void save_nodes(QSqlDatabase *db, const Execution *ex)
{
    const char *insert_query = "INSERT INTO Nodes \
                         (NodeID, ParentID, Alternative, NKids, Status, Label) \
                         VALUES (?,?,?,?,?,?);";

    QSqlQuery insert_bm(*db);
    insert_bm.prepare(insert_query);

    const auto &tree = ex->tree();
    const auto order = utils::pre_order(tree);

    constexpr static int TRANSACTION_SIZE = 50000;

    db->transaction();
    for (auto i = 0u; i < order.size(); ++i)
    {
        const auto nid = order[i];
        const auto pid = tree.getParent(nid);
        const auto alt = tree.getAlternative(nid);
        const auto kids = tree.childrenCount(nid);
        const auto status = tree.getStatus(nid);
        const auto label = tree.getLabel(nid);

        insert_node(&insert_bm, {nid, pid, alt, kids, status, label});

        if (i % TRANSACTION_SIZE == TRANSACTION_SIZE - 1)
        {
            db->commit();
            db->transaction();
        }
    }
    db->commit();
}

static void insert_bookmark(QSqlQuery *stmt, BookmarkItem bi)
{
    stmt->finish();

    stmt->addBindValue(static_cast<int>(bi.nid));
    stmt->addBindValue(bi.text.c_str());

    if (!stmt->exec())
    {
        print("ERROR: could not execute DB statement");
    }
}

static void insert_nogood(QSqlQuery *stmt, NogoodItem ngi)
{

    stmt->finish();

    stmt->addBindValue(static_cast<int>(ngi.nid));
    stmt->addBindValue(ngi.text.c_str());

    if (!stmt->exec())
    {
        print("ERROR: could not execute DB statement");
    }
}

static void save_nogoods(QSqlDatabase *db, const Execution *ex)
{
    const char *query = "INSERT INTO Nogoods \
                         (NodeID, Nogood) \
                         VALUES (?,?);";
    QSqlQuery insert_ng_stmt(*db);
    insert_ng_stmt.prepare(query);

    const auto &nt = ex->tree();
    const auto &sd = ex->solver_data();

    const auto nodes = utils::any_order(nt);


    db->transaction();

    for (const auto n : nodes)
    {
        /// TODO: should save renamed instead?
        const auto &text = sd.getNogood(n).original();
        if (text != "")
        {
            insert_nogood(&insert_ng_stmt, {n, text});
        }
    }

    db->commit();
}

static void insert_info(QSqlQuery *stmt, InfoItem ii)
{
    stmt->finish();

    stmt->addBindValue(static_cast<int>(ii.nid));
    stmt->addBindValue(ii.text.c_str());

    if (!stmt->exec())
    {
        print("ERROR: could not execute DB statement");
    }
}

static void save_info(QSqlDatabase *db, const Execution *ex)
{
    const char *query = "INSERT INTO Info \
                         (NodeID, Info) \
                         VALUES (?,?);";

    QSqlQuery insert_ng_stmt(*db);
    insert_ng_stmt.prepare(query);

    const auto &nt = ex->tree();
    const auto &sd = ex->solver_data();

    const auto nodes = utils::any_order(nt);

    db->transaction();
    for (const auto n : nodes)
    {
        const auto &text = sd.getInfo(n);
        if (text != "")
        {
            insert_info(&insert_ng_stmt, {n, text});
        }
    }
    db->commit();
}

static void save_user_data(QSqlDatabase *db, const Execution *ex)
{

    const char *query = "INSERT INTO Bookmarks \
                         (NodeID, Bookmark) \
                         VALUES (?,?);";

    QSqlQuery insert_bm_stmt(*db);
    insert_bm_stmt.prepare(query);

    const auto &ud = ex->userData();
    const auto nodes = ud.bookmarkedNodes();


    db->transaction();
    for (const auto n : nodes)
    {
        const auto &text = ud.getBookmark(n);
        insert_bookmark(&insert_bm_stmt, {n, text});
    }
    db->commit();
}

/// Create a file at `path` (or overwrite it) and associate it with `db`
static bool create_db(QSqlDatabase* db)
{
    const auto create_nodes = "CREATE TABLE Nodes( \
      NodeID INTEGER PRIMARY KEY, \
      ParentID int NOT NULL, \
      Alternative int NOT NULL, \
      NKids int NOT NULL, \
      Status int, \
      Label varchar(256) \
      );";

    const auto create_bookmarks = "Create TABLE Bookmarks( \
        NodeID INTEGER PRIMARY KEY, \
        Bookmark varchar(8) \
        );";

    const auto create_nogoods = "CREATE TABLE Nogoods( \
        NodeID INTEGER PRIMARY KEY, \
        Nogood varchar(8) \
    );";

    const auto create_info = "CREATE TABLE Info( \
        NodeID INTEGER PRIMARY KEY, \
        Info TEXT \
    );";

    QSqlQuery query (*db);

    if(!query.exec(create_nodes)) return false;
    if(!query.exec(create_bookmarks)) return false;
    if(!query.exec(create_nogoods)) return false;
    if(!query.exec(create_info)) return false;

    return true;
}

/// this takes (without nogoods) under 2 sec for a ~1.5M nodes (golomb 10)
void save_execution(const Execution *ex, const char *path)
{
    print("creating file: {}", path);
    std::ofstream file(path);
    file.close();

    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(path);

    if (!db.open()) {
        print("Cannot open database file.");
        return;
    }

    perfHelper.begin("save execution");

    if(!create_db(&db)) {
        print("Cannot create tables in database.");
        return;
    }

    save_nodes(&db, ex);

    save_user_data(&db, ex);

    const auto &sd = ex->solver_data();

    if (sd.hasNogoods())
    {
        save_nogoods(&db, ex);
    }

    if (sd.hasInfo())
    {
        save_info(&db, ex);
    }

    perfHelper.end();
}

std::shared_ptr<Execution> load_execution(const char *path, ExecID eid)
{
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(path);

    if (!db.open())
        return nullptr;

    auto ex = std::make_shared<Execution>(path, eid);

    read_nodes(&db, *ex);

    read_bookmarks(&db, *ex);

    read_nogoods(&db, *ex);

    read_info(&db, *ex);

    ex->tree().setDone();

    db.close();

    return ex;
}
} // namespace db_handler
} // namespace cpprofiler
