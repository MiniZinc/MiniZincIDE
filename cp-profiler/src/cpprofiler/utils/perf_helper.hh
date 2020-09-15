#ifndef CPPROFILER_UTILS_PERF_HELPER_HH
#define CPPROFILER_UTILS_PERF_HELPER_HH

#include <iostream>
#include <chrono>

#include <unordered_map>

#include "debug.hh"

namespace detail
{
using namespace std::chrono;

struct PerfInstance
{
    using TimePoint = time_point<high_resolution_clock>;
    long long current = 0;
    TimePoint begin;
};

class PerformanceHelper
{
    using TimePoint = time_point<high_resolution_clock>;

  private:
    high_resolution_clock m_hrClock;
    TimePoint m_begin;
    const char *m_message;

    std::unordered_map<const char *, PerfInstance> maps;

  public:
    void begin(const char *msg)
    {
        m_message = msg;
        m_begin = m_hrClock.now();
    }

    void accumulate(const char *msg)
    {
        maps[msg].begin = m_hrClock.now();
    }

    void end(const char *msg)
    {
        auto now = m_hrClock.now();
        auto duration_ns =
            duration_cast<nanoseconds>(now - maps[msg].begin).count();
        maps[msg].current += duration_ns;
        maps[msg].begin = now;
    }

    void total(const char *msg)
    {
        auto dur = maps[msg].current;
        std::cout << "Duration(" << msg << "): " << dur / 1000000 << "ms"
                  << " (" << dur << "ns)\n";
    }

    void end()
    {
        auto duration_ms =
            duration_cast<milliseconds>(m_hrClock.now() - m_begin).count();
        auto duration_ns =
            duration_cast<nanoseconds>(m_hrClock.now() - m_begin).count();
        ::cpprofiler::print("Duration({}): {}ms ({}ns)", m_message, duration_ms, duration_ns);
    }
};

} // namespace detail

namespace perf_helper
{

using namespace std::chrono;

struct Timer
{
    using TimePoint = time_point<high_resolution_clock>;
    high_resolution_clock m_hrClock;
    TimePoint m_begin;

    void begin()
    {
        m_begin = m_hrClock.now();
    }

    int64_t end()
    {
        int64_t ms =
            duration_cast<milliseconds>(m_hrClock.now() - m_begin).count();
        return ms;
    }
};

} // namespace perf_helper

extern detail::PerformanceHelper perfHelper;

#endif