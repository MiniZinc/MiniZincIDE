#ifndef CPPROFILER_RECEIVER_HH
#define CPPROFILER_RECEIVER_HH

#include <cstdint>
#include <memory>
#include <QThread>

namespace cpprofiler
{

class Conductor;
class Message;
class Execution;
class ReceiverWorker;
class Settings;

class ReceiverThread : public QThread
{
    Q_OBJECT
    const intptr_t m_socket_desc;
    std::unique_ptr<ReceiverWorker> m_worker;

    const Settings &m_settings;

    void run() override;

  signals:

    void notifyStart(const std::string &ex_name, int ex_id, bool restarts);
    void newNode(Message *node);
    void doneReceiving();

  public:
    ReceiverThread(intptr_t socket_desc, const Settings &s);
    ~ReceiverThread();
};

} // namespace cpprofiler

#endif