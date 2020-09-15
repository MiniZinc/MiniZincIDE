#include "conductor.hh"
#include "tcp_server.hh"
#include "receiver_thread.hh"
#include <iostream>
#include <thread>
#include <QTreeView>
#include <QGridLayout>
#include <QPushButton>
#include <QDebug>
#include <QFile>
#include <QFileDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QApplication>
#include "../cpp-integration/message.hpp"

#include "execution.hh"
#include "tree_builder.hh"
#include "execution_list.hh"
#include "execution_window.hh"

#include "tree/node_tree.hh"

#include "analysis/merge_window.hh"
#include "analysis/tree_merger.hh"

#include "utils/std_ext.hh"
#include "utils/string_utils.hh"
#include "utils/tree_utils.hh"
#include "utils/path_utils.hh"

#include "pixel_views/pt_canvas.hh"

#include <random>

#include <QProgressDialog>

#include "name_map.hh"
#include "db_handler.hh"

namespace cpprofiler
{

Conductor::Conductor(Options opt, QWidget* parent) : QMainWindow(parent), options_(opt)
{

    setWindowTitle("CP-Profiler");

    // readSettings();

    auto layout = new QGridLayout();

    {
        auto window = new QWidget();
        setCentralWidget(window);
        window->setLayout(layout);
    }

    execution_list_.reset(new ExecutionList);
    layout->addWidget(execution_list_->getWidget());

    auto showButton = new QPushButton("Show Tree");
    layout->addWidget(showButton);
    connect(showButton, &QPushButton::clicked, [this]() {
        for (auto execution : execution_list_->getSelected())
        {
            showTraditionalView(execution);
        }
    });

    auto mergeButton = new QPushButton("Merge Trees");
    layout->addWidget(mergeButton);

    connect(mergeButton, &QPushButton::clicked, [this]() {
        const auto selected = execution_list_->getSelected();

        if (selected.size() == 2)
        {
            mergeTrees(selected[0], selected[1]);
        }
        else
        {
            print("select exactly two executions");
        }
    });

    auto saveButton = new QPushButton("Save Execution");
    layout->addWidget(saveButton);

    /// TODO: make it clear that only the first one is saved
    connect(saveButton, &QPushButton::clicked, [this]() {
        for (auto e : execution_list_->getSelected())
        {
            saveExecution(e);
            break;
        }
    });

    auto loadButton = new QPushButton("Load Execution");
    layout->addWidget(loadButton);

    connect(loadButton, &QPushButton::clicked, [this]() {
        const auto fileName = QFileDialog::getOpenFileName(this, "Open Execution").toStdString();

        if (fileName == "")
            return;

        const auto eid = getNextExecId();

        auto ex = db_handler::load_execution(fileName.c_str(), eid);

        if (!ex)
        {
            print("could not load the execution");
        }
        else
        {
            const auto ex_id = addNewExecution(ex);
            print("New execution from a database, ex_id: {}", ex_id);
        }
    });

    server_.reset(new TcpServer([this](intptr_t socketDesc) {
        {
            /// Initiate a receiver thread
            auto receiver = new ReceiverThread(socketDesc, settings_);
            /// Delete the receiver one the thread is finished
            connect(receiver, &QThread::finished, receiver, &QObject::deleteLater);
            /// Handle the start message in this connector
            connect(receiver, &ReceiverThread::notifyStart, [this, receiver](const std::string &ex_name, int ex_id, bool restarts) {
                handleStart(receiver, ex_name, ex_id, restarts);
            });

            receiver->start();
        }
    }));

    listen_port_ = DEFAULT_PORT;

    // See if the default port is available
    auto res = server_->listen(QHostAddress::Any, listen_port_);
    if (!res)
    {
        // If not, try any port
        server_->listen(QHostAddress::Any, 0);
        listen_port_ = server_->serverPort();
    }

    std::cerr << "Ready to listen on: " << listen_port_ << std::endl;
}

static int getRandomExID()
{
    std::mt19937 rng;
    rng.seed(std::random_device()());
    std::uniform_int_distribution<std::mt19937::result_type> dist(100);

    return dist(rng);
}

void Conductor::onExecutionDone(Execution *e)
{
    e->tree().setDone();

    if (options_.save_search_path != "")
    {
        print("saving search to: {}", options_.save_search_path);
        saveSearch(e, options_.save_search_path.c_str());
        QApplication::quit();
    }

    if (options_.save_execution_db != "")
    {
        print("saving execution to db: {}", options_.save_execution_db);
        db_handler::save_execution(e, options_.save_execution_db.c_str());

        if(options_.save_pixel_tree_path != "") {
          print("Saving pixel tree to file: {}", options_.save_pixel_tree_path);
          savePixelTree(e, options_.save_pixel_tree_path.c_str(), options_.pixel_tree_compression);
        }

        QApplication::quit();
    }
}

int Conductor::getNextExecId() const
{
    int eid = getRandomExID();
    while (executions_.find(eid) != executions_.end())
    {
        eid = getRandomExID();
    }
    return eid;
}

void Conductor::setMetaData(int exec_id, const std::string &group_name,
                            const std::string &exec_name,
                            std::shared_ptr<NameMap> nm)
{
    exec_meta_.insert({exec_id, {group_name, exec_name, nm}});

    qDebug() << "exec_id:" << exec_id;
    qDebug() << "gr_name:" << group_name.c_str();
    qDebug() << "ex_name:" << exec_name.c_str();
}

int Conductor::getListenPort() const
{
    return static_cast<int>(listen_port_);
}

Conductor::~Conductor() = default;

void Conductor::handleStart(ReceiverThread *receiver, const std::string &ex_name, int ex_id, bool restarts)
{

    auto res = executions_.find(ex_id);

    if (res == executions_.end() || ex_id == 0)
    {

        /// Note: metadata from MiniZinc IDE overrides that provided by the solver
        std::string ex_name_used = ex_name;

        const bool ide_used = (exec_meta_.find(ex_id) != exec_meta_.end());

        if (ide_used)
        {
            print("already know metadata for this ex_id!");
            ex_name_used = exec_meta_[ex_id].ex_name;
        }

        /// needs a new execution
        auto ex = addNewExecution(ex_name_used, ex_id, restarts);

        /// construct a name map
        if (ide_used)
        {
            ex->setNameMap(exec_meta_[ex_id].name_map);
            print("using name map for {}", ex_id);
        }
        else if (options_.paths != "" && options_.mzn != "")
        {
            auto nm = std::make_shared<NameMap>();
            auto success = nm->initialize(options_.paths, options_.mzn);
            if (success)
            {
                ex->setNameMap(nm);
            }
        }

        /// The builder should only be created for a new execution
        auto builderThread = new QThread();
        auto builder = new TreeBuilder(*ex);

        builders_[ex_id] = builder;
        builder->moveToThread(builderThread);

	// onExecutionDone must be called on the same thread as the conductor
        connect(builder, &TreeBuilder::buildingDone, this, [this, ex]() {
            onExecutionDone(ex);
        });

        /// is this the right time to delete the builder thread?
        connect(builderThread, &QThread::finished, builderThread, &QObject::deleteLater);

        builderThread->start();
    }

    /// obtain the builder aready assigned to this execution
    ///(either just now or by another connection)
    auto builder = builders_[ex_id];

    connect(receiver, &ReceiverThread::newNode,
            builder, &TreeBuilder::handleNode);

    connect(receiver, &ReceiverThread::doneReceiving,
            builder, &TreeBuilder::finishBuilding);
}

int Conductor::addNewExecution(std::shared_ptr<Execution> ex)
{

    auto ex_id = getRandomExID();

    executions_[ex_id] = ex;
    execution_list_->addExecution(*ex);

    return ex_id;
}

Execution *Conductor::addNewExecution(const std::string &ex_name, int ex_id, bool restarts)
{

    if (ex_id == 0)
    {
        ex_id = getRandomExID();
    }

    auto ex = std::make_shared<Execution>(ex_name, ex_id, restarts);

    print("EXECUTION_ID: {}", ex_id);

    executions_[ex_id] = ex;
    execution_list_->addExecution(*ex);

    const bool auto_show = true;
    if (auto_show && options_.save_search_path == "" && options_.save_execution_db == "")
    {
        showTraditionalView(ex.get());
    }

    return ex.get();
}

ExecutionWindow &Conductor::getExecutionWindow(Execution *e)
{
    auto maybe_view = execution_windows_.find(e);

    /// create new one if doesn't already exist
    if (maybe_view == execution_windows_.end())
    {
        execution_windows_[e] = new ExecutionWindow(*e, this);

        const auto ex_window = execution_windows_[e];

        connect(ex_window, &ExecutionWindow::needToSaveSearch, [this, e]() {
            saveSearch(e);
        });

        connect(ex_window, &ExecutionWindow::nogoodsClicked, [this, e](std::vector<NodeID> ns) {
            emit computeHeatMap(e->id(), ns);
        });
    }

    return *execution_windows_.at(e);
}

void Conductor::mergeTrees(Execution *e1, Execution *e2)
{

    /// create new tree

    // QProgressDialog dialog;

    auto tree = std::make_shared<tree::NodeTree>();
    auto result = std::make_shared<analysis::MergeResult>();

    /// mapping from merged ids to original ids
    auto orig_locs = std::make_shared<std::vector<analysis::OriginalLoc>>();

    /// Note: TreeMerger will delete itself when finished
    auto merger = new analysis::TreeMerger(*e1, *e2, tree, result, orig_locs);

    connect(merger, &analysis::TreeMerger::finished, this,
            [this, e1, e2, tree, result]() {
                auto window = new analysis::MergeWindow(*e1, *e2, tree, result, this);
                emit showMergeWindow(*window);
                window->show();
            });

    merger->start();
}

void Conductor::runNogoodAnalysis(Execution *e1, Execution *e2)
{

    auto tree = std::make_shared<tree::NodeTree>();
    auto result = std::make_shared<analysis::MergeResult>();

    auto orig_locs = std::make_shared<std::vector<analysis::OriginalLoc>>();

    /// Note: TreeMerger will delete itself when finished
    auto merger = new analysis::TreeMerger(*e1, *e2, tree, result, orig_locs);

    connect(merger, &analysis::TreeMerger::finished, this,
            [this, e1, e2, tree, result]() {
                auto window = new analysis::MergeWindow(*e1, *e2, tree, result, this);
                emit showMergeWindow(*window);
                window->show();
                window->runNogoodAnalysis();
            });

    merger->start();
}

void Conductor::savePixelTree(Execution *e, const char* path, int compression_factor) const {
  const auto &nt = e->tree();
  pixel_view::PtCanvas pc(nt);
  auto pi = pc.get_pimage();
  pc.setCompression(compression_factor);
  int width  = pi->pixel_size()*pc.totalSlices();
  int height = pi->pixel_size()*nt.node_stats().maxDepth();
  pi->resize({width,height});
  pc.redrawAll(true);
  pi->raw_image().save(path);
}

void Conductor::saveSearch(Execution *e, const char *path) const
{

    const auto &nt = e->tree();

    const auto order = utils::pre_order(nt);

    QFile file(path);
    if (!file.open(QFile::WriteOnly | QFile::Truncate))
    {
        print("Error: could not open \"{}\" to save search", path);
        return;
    }

    QTextStream file_stream(&file);

    for (auto nid : order)
    {

        /// Making sure undefined/skipped nodes are not logged
        {
            const auto status = nt.getStatus(nid);
            if (status == tree::NodeStatus::SKIPPED || status == tree::NodeStatus::UNDETERMINED)
                continue;
        }

        // Note: not every child is logged (SKIPPED and UNDET are not)
        int kids_logged = 0;

        /// Note: this temporary stream is used so that children can be
        /// traversed first, counted, but logged after their parent
        std::stringstream children_stream;

        const auto kids = nt.childrenCount(nid);

        for (auto alt = 0; alt < kids; alt++)
        {

            const auto kid = nt.getChild(nid, alt);

            /// Making sure undefined/skipped children are not logged
            const auto status = nt.getStatus(kid);
            if (status == tree::NodeStatus::SKIPPED || status == tree::NodeStatus::UNDETERMINED)
                continue;

            ++kids_logged;
            /// TODO: use original names in labels
            const auto label = nt.getLabel(kid);

            children_stream << " " << kid << " " << label;
        }

        file_stream << nid << " " << kids_logged;

        /// Unexplored node on the left branch (search timed out)
        if ((kids == 0) && (nt.getStatus(nid) == tree::NodeStatus::BRANCH))
        {
            file_stream << " stop";
        }

        file_stream << children_stream.str().c_str() << '\n';
    }
}

void Conductor::saveSearch(Execution *e) const
{

    const auto file_path = QFileDialog::getSaveFileName(nullptr, "Save Search To a File").toStdString();

    saveSearch(e, file_path.c_str());
}

void Conductor::saveExecution(Execution *e)
{

    const auto file_path = QFileDialog::getSaveFileName(nullptr, "Save Execution To a File").toStdString();

    db_handler::save_execution(e, file_path.c_str());

    qDebug() << "execution saved";
}

void Conductor::readSettings()
{

    QFile file("settings.json");

    if (!file.exists())
    {
        qDebug() << "settings.json not found";
        return;
    }

    file.open(QIODevice::ReadWrite | QIODevice::Text);

    auto data = file.readAll();

    auto json_doc = QJsonDocument::fromJson(data);

    if (json_doc.isEmpty())
    {
        qDebug() << "settings.json is empty";
        return;
    }

    if (json_doc.isObject())
    {

        auto json_obj = json_doc.object();

        settings_.receiver_delay = json_obj["receiver_delay"].toInt();
    }

    qDebug() << "settings read";
}

static std::string getHeatMapUrl(const NameMap &nm,
                                 const std::unordered_map<int, int> &con_counts,
                                 int max_count)
{
    /// get heat map

    std::unordered_map<std::string, int> loc_intensity;

    for (const auto& it : con_counts)
    {
        const auto con = std::to_string(it.first);
        const auto count = it.second;

        const auto &path = nm.getPath(con);

        const auto path_head_elements = utils::getPathPair(path, true).model_level;

        if (path_head_elements.empty())
            continue;

        const auto path_head = path_head_elements.back();

        const auto location_etc = utils::split(path_head, utils::minor_sep);

        // print("path: {}", path);

        // for (auto e : new_loc) {
        //     print("element: {}", e);
        // }

        /// path plus four ints
        if (location_etc.size() < 5)
            continue;

        std::vector<std::string> new_loc(location_etc.begin(), location_etc.begin() + 5);

        int val = static_cast<int>(std::floor(count * (255.0 / max_count)));

        const auto loc_str = utils::join(new_loc, utils::minor_sep);

        loc_intensity[loc_str] = std::max(loc_intensity[loc_str], val);
    }

    /// highlight url
    std::stringstream url;
    url << "highlight://?";

    for (auto it : loc_intensity)
        url << it.first << utils::minor_sep << it.second << ";";

    return url.str();
}

void Conductor::computeHeatMap(ExecID eid, std::vector<NodeID> ns)
{

    auto it = exec_meta_.find(eid);

    if (it == exec_meta_.end())
    {
        print("No metadata for eid {} (ExecMeta)", eid);
        return;
    }

    /// check if namemap is there
    const auto nm = it->second.name_map;

    if (!nm)
    {
        print("no name map for eid: {}", eid);
        return;
    }

    const auto exec = executions_.at(eid);

    const auto &sd = exec->solver_data();

    std::unordered_map<int, int> con_counts;

    for (const auto& n : ns)
    {
        const auto *cs = sd.getContribConstraints(n);

        if (!cs)
            continue;

        for (int con_id : *cs)
        {
            con_counts[con_id]++;
        }
    }

    int max_count = 0;
    for (const auto& p : con_counts)
    {
        if (p.second > max_count)
        {
            max_count = p.second;
        }
    }

    const auto url = getHeatMapUrl(*nm, con_counts, max_count);

    if (url.empty())
        return;

    std::stringstream label;

    for (const auto n : ns)
    {
        label << std::to_string(n) << ' ';
    }

    emit showNogood(url.c_str(), label.str().c_str(), false);
}

} // namespace cpprofiler

namespace cpprofiler
{

void Conductor::showTraditionalView(Execution *e)
{

    auto &window = getExecutionWindow(e);

    emit showExecutionWindow(window);
    window.show();
}
} // namespace cpprofiler
