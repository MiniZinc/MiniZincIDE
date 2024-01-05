#ifndef CONNECTOR
#define CONNECTOR

#include "message.hpp"

#include <iostream>
#include <sstream>
#include <vector>
#include <cstring>

#ifdef WIN32

#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Mswsock.lib")
#pragma comment(lib, "AdvApi32.lib")

#include <basetsd.h>
typedef SSIZE_T ssize_t;

#else

#include <netdb.h>
#include <unistd.h>

#endif

namespace cpprofiler {

template <typename T>
class Option {
  T value_;
  bool present{false};

public:
  bool valid() const { return present; }
  void set(const T& t) { present = true; value_ = t; }
  void unset() { present = false; }
  const T& value() const { assert(present); return value_; }
  T& value() { assert(present); return value_; }
};

class Connector;
class Node;
static void sendNode(Connector& c, Node& node);

class Node {
  Connector& _c;

  NodeUID node_;
  NodeUID parent_;
  int alt_;
  int kids_;

  NodeStatus status_;

  Option<std::string> label_;
  Option<std::string> nogood_;
  Option<std::string> info_;

public:
  Node(NodeUID node, NodeUID parent,
       int alt, int kids, NodeStatus status, Connector& c)
    : _c(c), node_{node}, parent_{parent},
      alt_(alt), kids_(kids), status_(status) {}

  Node& set_node_thread_id(int tid) {
    node_.tid = tid;
    return *this;
  }

  const Option<std::string>& label() const { return label_; }

  Node& set_label(const std::string& label) {
    label_.set(label);
    return *this;
  }

  const Option<std::string>& nogood() const { return nogood_; }

  Node& set_nogood(const std::string& nogood) {
    nogood_.set(nogood);
    return *this;
  }

  const Option<std::string>& info() const { return info_; }

  Node& set_info(const std::string& info) {
    info_.set(info);
    return *this;
  }

  int alt() const { return alt_; }
  int kids() const { return kids_; }

  NodeStatus status() const { return status_; }

  NodeUID nodeUID() const { return node_; }
  NodeUID parentUID() const { return parent_; }

  int node_id() const { return node_.nid; }
  int parent_id() const { return parent_.nid; }
  int node_thread_id() const {  return node_.tid; }
  int node_restart_id() const { return node_.rid; }
  int parent_thread_id() const {  return parent_.tid; }
  int parent_restart_id() const { return parent_.rid; }

  void send() { sendNode(_c, *this); }
};

// From http://beej.us/guide/bgnet/output/html/multipage/advanced.html#sendall
static int sendall(int s, const char* buf, int* len) {
  int total = 0;         // how many bytes we've sent
  int bytesleft = *len;  // how many we have left to send
  ssize_t n;

  while (total < *len) {
    n = send(s, buf + total, static_cast<size_t>(bytesleft), 0);
    if (n == -1) {
      break;
    }
    total += n;
    bytesleft -= n;
  }

  *len = total;  // return number actually sent here

  return n == -1 ? -1 : 0;  // return -1 on failure, 0 on success
}

class Connector {
private:
  MessageMarshalling marshalling;

  const unsigned int port;

  int sockfd;
  bool _connected;

  void sendOverSocket() {
    if (!_connected) return;

    std::vector<char> buf = marshalling.serialize();

    sendRawMsg(buf);
  }

public:
  void sendRawMsg(const std::vector<char>& buf) {
    uint32_t bufSize = static_cast<uint32_t>(buf.size());
    int bufSizeLen = sizeof(uint32_t);
    sendall(sockfd, reinterpret_cast<char*>(&bufSize), &bufSizeLen);
    int bufSizeInt = static_cast<int>(bufSize);
    sendall(sockfd, reinterpret_cast<const char*>(buf.data()), &bufSizeInt);
  }

  Connector(unsigned int port) : port(port), _connected(false) {}

  bool connected() { return _connected; }

  /// connect to a socket via port specified in the construction (6565 by
  /// default)
  void connect() {
    struct addrinfo hints, *servinfo, *p;
    int rv;

#ifdef WIN32
    // Initialise Winsock.
    WSADATA wsaData;
    int startupResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (startupResult != 0) {
      printf("WSAStartup failed with error: %d\n", startupResult);
    }
#endif

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo("localhost", std::to_string(port).c_str(), &hints,
                          &servinfo)) != 0) {
      std::cerr << "getaddrinfo: " << gai_strerror(rv) << "\n";
      goto giveup;
    }

    // loop through all the results and connect to the first we can
    for (p = servinfo; p != NULL; p = p->ai_next) {
      if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
        // errno is set here, but we don't examine it.
        continue;
      }

      if (::connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
#ifdef WIN32
        closesocket(sockfd);
#else
        close(sockfd);
#endif
        // errno is set here, but we don't examine it.
        continue;
      }

      break;
    }

    // Connection failed; give up.
    if (p == NULL) {
      goto giveup;
    }

    freeaddrinfo(servinfo);  // all done with this structure

    _connected = true;

    return;
giveup:
    _connected = false;
    return;

  }

  // sends START_SENDING message to the Profiler with a model name
  void start(const std::string& file_path = "",
               int execution_id = -1, bool has_restarts = false) {
    /// extract fzn file name
    std::string base_name(file_path);
    {
      size_t pos = base_name.find_last_of('/');
      if (pos != static_cast<size_t>(-1)) {
        base_name = base_name.substr(pos + 1, base_name.length() - pos - 1);
      }
    }

    std::string info{""};
    {
      std::stringstream ss;
      ss << "{";
      ss << "\"has_restarts\": " << (has_restarts ? "true" : "false")  << "\n";
      ss << ",\"name\": " << "\"" << base_name << "\"" << "\n";
      if (execution_id != -1) {
        ss << ",\"execution_id\": " << execution_id;
      }
      ss << "}";
      info = ss.str();
    }

    marshalling.makeStart(info);
    sendOverSocket();
  }

  void restart(int restart_id = -1) {

    std::string info{""};
    {
      std::stringstream ss;
      ss << "{";
      ss << "\"restart_id\": " << restart_id << "\n";
      ss << "}";
      info = ss.str();
    }

    marshalling.makeRestart(info);
    sendOverSocket();
  }

  void done() {
    marshalling.makeDone();
    sendOverSocket();
  }

  /// disconnect from a socket
  void disconnect() {
#ifdef WIN32
    closesocket(sockfd);
#else
    close(sockfd);
#endif
  }

  void sendNode(const Node& node) {
    if (!_connected) return;

    auto& msg = marshalling.makeNode(node.nodeUID(), node.parentUID(),
                                     node.alt(), node.kids(), node.status());

    if (node.label().valid()) msg.set_label(node.label().value());
    if (node.nogood().valid()) msg.set_nogood(node.nogood().value());
    if (node.info().valid()) msg.set_info(node.info().value());

    sendOverSocket();
  }

  Node createNode(NodeUID node, NodeUID parent,
                  int alt, int kids, NodeStatus status) {
    return Node(node, parent, alt, kids, status, *this);
  }
};

void sendNode(Connector& c, Node& node) { c.sendNode(node); }

}

#endif
