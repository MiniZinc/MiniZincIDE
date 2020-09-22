#include "receiver_worker.hh"
#include "conductor.hh"
#include <iostream>
#include <string>
#include <thread>
#include <QTcpSocket>
#include <QJsonObject>
#include <QJsonDocument>

#include "execution.hh"
#include "utils/utils.hh"

#include "tree/node.hh"
#include "settings.hh"

namespace cpprofiler
{

ReceiverWorker::ReceiverWorker(QTcpSocket &socket, const Settings &s)
    : m_socket(socket), m_settings(s)
{
}

static int32_t ArrayToInt(const QByteArray &ba)
{
    const char *p = ba.data();
    return *(reinterpret_cast<const quint32 *>(p));
}

void ReceiverWorker::doRead()
{
    // are there enough bytes to read something
    bool can_read_more = true;

    while (m_socket.bytesAvailable() > 0 || can_read_more)
    {

        if (m_socket.bytesAvailable() > 0)
            can_read_more = true;

        m_buffer.append(m_socket.readAll());

        // read the size of the next field if haven't already
        if (!m_state.size_read)
        {

            if (m_buffer.size() < m_state.bytes_read + FIELD_SIZE_NBYTES)
            {
                /// can't read, need to wait for more bytes
                can_read_more = false;
                continue;
            }

            /// enough bytes to read size
            m_state.msg_size = ArrayToInt(m_buffer.mid(m_state.bytes_read, 4));
            m_state.bytes_read += 4;
            m_state.size_read = true;
        }
        else
        {

            if (m_buffer.size() < m_state.bytes_read + m_state.msg_size)
            {
                /// can't read, need to wait for more bytes
                can_read_more = false;
                continue;
            }

            marshalling.deserialize(m_buffer.data() + m_state.bytes_read, m_state.msg_size);

            auto msg = marshalling.get_msg();
            handleMessage(msg);

            m_state.bytes_read += m_state.msg_size;
            m_state.size_read = false;

            m_state.msg_processed++;

            /// reset the buffer every MSG_PER_BUFFER messages
            if (m_state.msg_processed == MSG_PER_BUFFER)
            {
                m_state.msg_processed = 0;
                m_buffer.remove(0, m_state.bytes_read);
                m_state.bytes_read = 0;
            }
        }
    }
}

void ReceiverWorker::handleStart(const Message &msg)
{

    /// create new execution

    std::string execution_name = "<no name>";
    bool has_restarts = false;

    int exec_id = -1;

    if (msg.has_info())
    {

        auto info_bytes = QByteArray::fromStdString(msg.info());
        auto json_doc = QJsonDocument::fromJson(info_bytes);

        if (json_doc.isNull() || json_doc.isArray())
        {
            print("Warning: start message info is invalid or empty");
        }
        else
        {
            QJsonObject json_obj = json_doc.object();

            auto name_val = json_obj.value("name");

            if (name_val.isString())
            {
                execution_name = name_val.toString().toStdString();
            }

            auto restarts_val = json_obj.value("has_restarts");

            if (restarts_val.isBool())
            {
                has_restarts = restarts_val.toBool();
            }

            {
                auto e_id_val = json_obj.value("execution_id");
                exec_id = e_id_val.toInt();
            }
        }
    }

    print("New execution: (name: {}, exec_id: {}, has restarts: {}", execution_name, exec_id, has_restarts);

    emit notifyStart(execution_name, exec_id, has_restarts); // blocking connection
}

/// nid, pid, alt, kids are needed during the Tree Construction
/// after the tree is build, these fields are easier to get from the tree structure

/// label, nogood and info can be quieried at any time

void ReceiverWorker::handleMessage(const Message &msg)
{

    if (m_settings.receiver_delay > 0)
    {
        utils::sleep_for_ms(m_settings.receiver_delay);
    }

    switch (msg.type())
    {
    case cpprofiler::MsgType::NODE:

        try
        {
            /// NOTE(maxim): the messae will be deleted by the builder thread
            auto node_msg = new Message{msg};
            emit newNode(node_msg);
        }
        catch (std::exception &e)
        {
        }

        break;
    case cpprofiler::MsgType::START:
        print("message: start");
        handleStart(msg);
        break;
    case cpprofiler::MsgType::DONE:
        emit doneReceiving();
        print("message: done");
        break;
    case cpprofiler::MsgType::RESTART:
        print("message: restart");
        break;
    default:
        print("ERROR: unknown solver message");
    }
}

} // namespace cpprofiler