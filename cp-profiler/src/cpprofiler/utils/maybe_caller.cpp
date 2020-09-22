#include "maybe_caller.hh"
#include <iostream>

namespace cpprofiler
{
namespace utils
{

MaybeCaller::MaybeCaller(int min_ms) : min_elapsed_(min_ms)
{
    updateTimer.setSingleShot(true);
    connect(&updateTimer, SIGNAL(timeout()), this, SLOT(callViaTimer()));
    last_call_time = std::chrono::system_clock::now();
}

void MaybeCaller::call(std::function<void(void)> fn)
{
    using namespace std::chrono;

    auto now = system_clock::now();
    auto elapsed = duration_cast<milliseconds>(now - last_call_time).count();

    if (elapsed > min_elapsed_)
    {
        fn();
        last_call_time = system_clock::now();
    }
    else
    {
        /// call delayed
        delayed_fn = fn;
        updateTimer.start(min_elapsed_);
    }
}

void MaybeCaller::callViaTimer() { delayed_fn(); }

} // namespace utils
} // namespace cpprofiler