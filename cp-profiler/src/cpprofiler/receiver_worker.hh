#ifndef CPPROFILER_RECEIVER_WORKER_HH
#define CPPROFILER_RECEIVER_WORKER_HH

#include <QObject>
#include <memory>

#include "../cpp-integration/message.hpp"

class QTcpSocket;

namespace cpprofiler
{

class Conductor;
class Execution;
class Message;
class Settings;

class ReceiverWorker : public QObject
{
    Q_OBJECT

    /// number of messages to read before resetting the buffer
    static constexpr int MSG_PER_BUFFER = 10000;
    /// the number of bytes per field size
    static constexpr int FIELD_SIZE_NBYTES = 4;

    /// read buffer
    QByteArray m_buffer;

    QTcpSocket &m_socket;

    struct ReadState
    {
        /// whether the size has been read
        bool size_read = false;
        // size of the current message in bytes
        int msg_size = 0;
        /// messages processed since last buffer reset
        int msg_processed = 0;
        /// where to read next from in the buffer
        int bytes_read = 0;
    } m_state;

    // Execution* execution;

    cpprofiler::MessageMarshalling marshalling;

    void handleStart(const cpprofiler::Message &msg);

    void handleMessage(const cpprofiler::Message &msg);

    const Settings &m_settings;

  signals:

    void notifyStart(const std::string &ex_name, int ex_id, bool restarts);
    void newNode(Message *node);
    void doneReceiving();

  public:
    ReceiverWorker(QTcpSocket &socket, const Settings &s);
  public slots:
    void doRead();
};

} // namespace cpprofiler

#endif