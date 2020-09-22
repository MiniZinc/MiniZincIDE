#ifndef CPPROFILER_TCP_SERVER_HH
#define CPPROFILER_TCP_SERVER_HH

#include <QTcpServer>
#include <functional>
#include <cstdint>

namespace cpprofiler
{

class TcpServer : public QTcpServer
{
    Q_OBJECT
  public:
    TcpServer(std::function<void(intptr_t)> callback);

  private:
    void incomingConnection(qintptr socketDesc) override;

    std::function<void(intptr_t)> m_callback;
};

} // namespace cpprofiler

#endif