
#include "execution.hh"
#include "name_map.hh"
#include "tree/node_tree.hh"
#include "user_data.hh"
#include "utils/debug.hh"

#include <iostream>

namespace cpprofiler
{

std::string Execution::name() { return name_; }

Execution::Execution(const std::string &name, ExecID id, bool restarts)
    : id_(id), name_{name}, tree_{new tree::NodeTree()},
      solver_data_(utils::make_unique<SolverData>()),
      user_data_(utils::make_unique<UserData>()), m_is_restarts(restarts)
{
    tree_->setSolverData(solver_data_);

    /// need to create a dummy root node
    if (restarts)
    {
        print("restart execution!");
        tree_->createRoot(0, "root");
    }
}

void Execution::setNameMap(std::shared_ptr<const NameMap> nm)
{
    name_map_ = nm;
    tree_->setNameMap(nm);
}

bool Execution::doesRestarts() const { return m_is_restarts; }

} // namespace cpprofiler