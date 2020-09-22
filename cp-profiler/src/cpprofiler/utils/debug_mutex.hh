
#ifndef CPPROFILER_UTILS_DEBUG_MUTEX_HH
#define CPPROFILER_UTILS_DEBUG_MUTEX_HH

#include <QMutex>
#include <QDebug>

namespace cpprofiler
{
namespace utils
{

class DebugMutex : public QMutex
{
  public:
    DebugMutex() : QMutex(QMutex::Recursive) {}

    void lock(std::string msg)
    {

        // qDebug() << "(" << msg.c_str() << ") lock";

        QMutex::lock();
    }

    bool tryLock()
    {

        // qDebug() << "try lock";
        return QMutex::tryLock();
    }

    void unlock(std::string msg)
    {
        // qDebug() << "(" << msg.c_str() << ") unlock";
        QMutex::unlock();
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