#include "execution_list.hh"
#include "execution.hh"

#include <QDebug>
#include <iostream>

namespace cpprofiler
{

ExecutionItem::ExecutionItem(Execution &e)
    : QStandardItem(e.name().c_str()), m_execution(e) {}

Execution *ExecutionItem::get_execution()
{
    return &m_execution;
}
} // namespace cpprofiler

namespace cpprofiler
{

ExecutionList::ExecutionList()
{
    m_tree_view.setHeaderHidden(true);
    m_tree_view.setModel(&m_execution_tree_model);
    m_tree_view.setSelectionMode(QAbstractItemView::MultiSelection);
}

void ExecutionList::addExecution(Execution &e)
{
    m_execution_tree_model.appendRow(new ExecutionItem{e});
}

std::vector<Execution *> ExecutionList::getSelected()
{
    auto selected_model = m_tree_view.selectionModel();
    auto selected = selected_model->selectedRows();

    std::vector<Execution *> result;

    for (auto idx : selected)
    {

        auto item = m_execution_tree_model.itemFromIndex(idx);
        auto exec_item = dynamic_cast<ExecutionItem *>(item);

        if (exec_item)
        {
            result.push_back(exec_item->get_execution());
        }
    }

    return result;
}

} // namespace cpprofiler