
#ifndef CPPROFILER_UTILS_DEBUG_MUTEX_HH
#define CPPROFILER_UTILS_DEBUG_MUTEX_HH

#if QT_VERSION >= 0x060000
#include <QRecursiveMutex>
#else
#include <QMutex>
#endif
#include <QDebug>

namespace cpprofiler
{
namespace utils
{

#if QT_VERSION >= 0x060000
typedef QRecursiveMutex MyMutex;
#else
typedef QMutex MyMutex;
#endif
class DebugMutex : public MyMutex
{
  public:
    DebugMutex() {}

    void lock(std::string msg)
    {

        // qDebug() << "(" << msg.c_str() << ") lock";

        MyMutex::lock();
    }

    bool tryLock()
    {

        // qDebug() << "try lock";
        return MyMutex::tryLock();
    }

    void unlock(std::string msg)
    {
        // qDebug() << "(" << msg.c_str() << ") unlock";
        MyMutex::unlock();
    }
};

class DebugMutexLocker
{

    DebugMutex *m_mutex;

    std::string m_msg;

  public:
    DebugMutexLocker(DebugMutex *m, std::string msg = "") : m_mutex(m), m_msg(msg)
    {
        m_mutex->lock(m_msg);
    }

    ~DebugMutexLocker()
    {
        m_mutex->unlock(m_msg);
    }
};

// using Mutex = QMutex;
// using MutexLocker = QMutexLocker;
using Mutex = utils::DebugMutex;
using MutexLocker = utils::DebugMutexLocker;

} // namespace utils
} // namespace cpprofiler

#endif