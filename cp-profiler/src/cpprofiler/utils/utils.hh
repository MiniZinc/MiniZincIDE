#ifndef CPPROFILER_UTILS_HH
#define CPPROFILER_UTILS_HH

#include <memory>
#include <ostream>
#include <unordered_map>

namespace cpprofiler
{
namespace utils
{

template <typename K, typename T>
void print(std::ostream &os, const std::unordered_map<K, T> &map)
{
    for (auto &k : map)
    {
        os << k.first << "\t" << k.second << "\n";
    }
}

void sleep_for_ms(int ms);

} // namespace utils
} // namespace cpprofiler

#endif