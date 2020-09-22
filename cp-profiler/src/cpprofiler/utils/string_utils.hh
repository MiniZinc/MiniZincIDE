#include <vector>
#include <string>

namespace cpprofiler
{
namespace utils
{

std::vector<std::string> split(const std::string &str, char delim, bool include_empty = false);
std::string join(const std::vector<std::string>& strs, char sep);
}
} // namespace cpprofiler