#pragma once

#include "core.hh"

namespace cpprofiler
{

class Execution;

namespace db_handler
{

/// Save existing execution `ex` to a file at `path`
void save_execution(const Execution *ex, const char *path);

/// Create a new execution based on a db file at `path`
std::shared_ptr<Execution> load_execution(const char *path, ExecID eid = 0);
} // namespace db_handler

} // namespace cpprofiler