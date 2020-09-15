#include "tcp_server.hh"

namespace cpprofiler
{

TcpServer::TcpServer(std::function<void(intptr_t)> callback)
    : QTcpServer{}, m_callback(callback) {}

void TcpServer::incomingConnection(qintptr handle)
{
    m_callback(handle);
}

} // namespace cpprofiler