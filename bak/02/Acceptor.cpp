#include "Acceptor.h"

static constexpr uint32_t kListenEvents = EPOLLIN | EPOLLERR;

Acceptor::Acceptor() {}

Acceptor::~Acceptor() { stop(); }

bool Acceptor::start(EventLoop* loop, uint16_t port) {
  if (!loop || listen_channel_) return false;
  loop_ = loop;
  port_ = port;

  if (!listen_sock_.create()) {
    std::cerr << "Acceptor socket failed: " << std::strerror(errno) << "\n";
    return false;
  }
  if (!listen_sock_.setReuseAddr(true)) {
    std::cerr << "Acceptor setReuseAddr failed: " << std::strerror(errno) << "\n";
    listen_sock_.close();
    return false;
  }

  InetAddress addr(port_);
  if (!listen_sock_.bind(addr)) {
    std::cerr << "Acceptor bind failed: " << std::strerror(errno) << "\n";
    listen_sock_.close();
    return false;
  }
  if (!listen_sock_.listen(SOMAXCONN)) {
    std::cerr << "Acceptor listen failed: " << std::strerror(errno) << "\n";
    listen_sock_.close();
    return false;
  }
  if (!listen_sock_.setNonBlock()) {
    std::cerr << "Acceptor setNonBlock failed: " << std::strerror(errno) << "\n";
    listen_sock_.close();
    return false;
  }

  listen_channel_ = new Channel(listen_sock_.fd());
  listen_channel_->setEvents(kListenEvents);
  listen_channel_->setReadCallback(std::bind(&Acceptor::handleAccept, this));
  listen_channel_->setErrorCallback([]() { std::cerr << "listen fd error\n"; });

  if (!loop_->addChannel(listen_channel_)) {
    std::cerr << "Acceptor addChannel failed: " << std::strerror(errno) << "\n";
    delete listen_channel_;
    listen_channel_ = nullptr;
    listen_sock_.close();
    return false;
  }

  std::cout << "Acceptor listening on 0.0.0.0:" << port_ << "\n";
  return true;
}

void Acceptor::stop() {
  if (!loop_ || !listen_channel_) {
    listen_sock_.close();
    return;
  }
  loop_->removeChannel(listen_channel_);
  delete listen_channel_;
  listen_channel_ = nullptr;
  loop_ = nullptr;
  listen_sock_.close();
}

void Acceptor::handleAccept() {
  if (!newConnectionCallback_) return;
  while (listen_channel_) {
    sockaddr_in cli{};
    socklen_t len = sizeof(cli);
    int cfd = ::accept4(listen_sock_.fd(), reinterpret_cast<sockaddr*>(&cli), &len,
                        SOCK_NONBLOCK | SOCK_CLOEXEC);
    if (cfd < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) break;
      if (errno == EINTR) continue;
      std::cerr << "accept4 failed: " << std::strerror(errno) << "\n";
      break;
    }
    newConnectionCallback_(cfd);
  }
}
