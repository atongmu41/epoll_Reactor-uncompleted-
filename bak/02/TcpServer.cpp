#include "TcpServer.h"

#include <functional>

TcpServer::TcpServer(uint16_t port)
    : port_(port), loop_(), acceptor_(), running_(false) {}

TcpServer::~TcpServer() { stop(); }

bool TcpServer::start() {
  if (running_) return true;

  ::signal(SIGPIPE, SIG_IGN);

  acceptor_.setNewConnectionCallback(
      std::bind(&TcpServer::onNewConnection, this, std::placeholders::_1));

  if (!acceptor_.start(&loop_, port_)) {
    return false;
  }

  running_ = true;
  return true;
}

void TcpServer::stop() {
  running_ = false;

  if (loop_.valid()) {
    for (auto it = connections_.begin(); it != connections_.end();) {
      Connection* c = it->second;
      ++it;
      removeConnection(c);
    }
    acceptor_.stop();
    loop_.close();
  }
}

void TcpServer::loop() {
  if (!running_) {
    if (!start()) return;
  }
  loop_.run();
  stop();
}

void TcpServer::onNewConnection(int fd) {
  Connection* conn = new Connection(&loop_, fd);
  conn->setCloseCallback(
      std::bind(&TcpServer::removeConnection, this, std::placeholders::_1));

  if (!conn->establish()) {
    std::cerr << "Connection establish failed for fd=" << fd << "\n";
    delete conn;
    ::close(fd);
    return;
  }
  connections_[fd] = conn;
}

void TcpServer::removeConnection(Connection* conn) {
  if (!conn) return;
  int fd = conn->fd();
  connections_.erase(fd);
  delete conn;
}
