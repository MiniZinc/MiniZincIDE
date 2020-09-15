#include "utils.hh"

#include <thread>
#include <chrono>

namespace cpprofiler
{
namespace utils
{

void sleep_for_ms(int ms)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

} // namespace utils
} // namespace cpprofiler
