#pragma once

namespace cpprofiler
{

struct Options
{
    std::string paths;
    std::string mzn;
    std::string save_search_path;
    std::string save_execution_db;
    std::string save_pixel_tree_path;
    int pixel_tree_compression;
};

} // namespace cpprofiler
