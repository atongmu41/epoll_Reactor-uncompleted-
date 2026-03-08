#include "TcpServer.h"

static constexpr uint32_t kListenEvents = EPOLLIN | EPOLLERR;
static constexpr uint32_t kConnEventsR =
    EPOLLIN | EPOLLRDHUP | EPOLLERR;
static constexpr uint32_t kConnEventsRW =
    EPOLLIN | EPOLLOUT | EPOLLRDHUP | EPOLLERR;

TcpServer::TcpServer(uint16_t port)
    : port_(port), listen_sock_(), loop_(), running_(false), listen_channel_(nullptr) {}

TcpServer::~TcpServer() { stop(); }

bool TcpServer::start() {
  if (running_) return true;

  ::signal(SIGPIPE, SIG_IGN);

  if (!listen_sock_.create()) {
    std::cerr << "socket failed: " << std::strerror(errno) << "\n";
    stop();
    return false;
  }
  if (!listen_sock_.setReuseAddr(true)) {
    std::cerr << "set SO_REUSEADDR failed: " << std::strerror(errno) << "\n";
    stop();
    return false;
  }

  InetAddress listen_addr(port_);
  if (!listen_sock_.bind(listen_addr)) {
    std::cerr << "bind failed: " << std::strerror(errno) << "\n";
    stop();
    return false;
  }
  if (!listen_sock_.listen(SOMAXCONN)) {
    std::cerr << "listen failed: " << std::strerror(errno) << "\n";
    stop();
    return false;
  }
  if (!listen_sock_.setNonBlock()) {
    std::cerr << "set_nonblocking(listen_fd) failed: " << std::strerror(errno) << "\n";
    stop();
    return false;
  }

  listen_channel_ = new Channel(listen_sock_.fd());
  listen_channel_->setEvents(kListenEvents);
  listen_channel_->setReadCallback(std::bind(&TcpServer::handle_accept, this));
  listen_channel_->setErrorCallback([this]() { std::cerr << "listen fd error\n"; });

  if (!loop_.addChannel(listen_channel_)) {
    std::cerr << "EventLoop addChannel listen_fd failed: " << std::strerror(errno) << "\n";
    stop();
    return false;
  }

  running_ = true;
  std::cout << "tcp server listening on 0.0.0.0:" << port_ << "\n";
  return true;
}

void TcpServer::stop() {
  running_ = false;

  if (loop_.valid()) {
    for (auto it = channels_.begin(); it != channels_.end();) {
      int fd = it->first;
      ++it;
      close_conn(fd);
    }
    if (listen_channel_) {
      loop_.removeChannel(listen_channel_);
      delete listen_channel_;
      listen_channel_ = nullptr;
    }
    loop_.close();
  }
  listen_sock_.close();
}

void TcpServer::loop() {
  if (!running_) {
    if (!start()) return;
  }
  loop_.run();
  stop();
}

void TcpServer::close_conn(int fd) {
  auto it = channels_.find(fd);
  if (it != channels_.end()) {
    Channel* ch = it->second;
    if (loop_.valid()) loop_.removeChannel(ch);
    ::close(fd);
    outbuf_.erase(fd);
    delete ch;
    channels_.erase(it);
  }
}

void TcpServer::handle_accept() {
  while (running_) {
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

    Channel* ch = new Channel(cfd);
    ch->setEvents(kConnEventsR);
    ch->setReadCallback(std::bind(&TcpServer::handle_read, this, cfd));
    ch->setWriteCallback(std::bind(&TcpServer::handle_write, this, cfd));
    ch->setErrorCallback(std::bind(&TcpServer::close_conn, this, cfd));
    ch->setCloseCallback(std::bind(&TcpServer::close_conn, this, cfd));

    if (!loop_.addChannel(ch)) {
      std::cerr << "EventLoop addChannel conn failed: " << std::strerror(errno) << "\n";
      ::close(cfd);
      delete ch;
      continue;
    }
    channels_.emplace(cfd, ch);
    outbuf_.emplace(cfd, std::string{});
  }
}

void TcpServer::handle_read(int fd) {
  auto it = outbuf_.find(fd);
  if (it == outbuf_.end()) return;

  char buf[4096];
  std::string recv_buf;
  while (running_) {
    ssize_t r = ::recv(fd, buf, sizeof(buf), 0);
    if (r > 0) {
      it->second.append(buf, static_cast<size_t>(r));
      recv_buf.append(buf, static_cast<size_t>(r));
      continue;
    }
    if (r == 0) {
      close_conn(fd);
      return;
    }
    if (errno == EINTR) continue;
    if (errno == EAGAIN || errno == EWOULDBLOCK) break;
    close_conn(fd);
    return;
  }

  if (!recv_buf.empty()) {
    std::cout << "server recv(fd=" << fd << "): " << recv_buf << "\n";
  }

  it = outbuf_.find(fd);
  if (it != outbuf_.end() && !it->second.empty()) {
    auto it_ch = channels_.find(fd);
    if (it_ch != channels_.end()) {
      Channel* ch = it_ch->second;
      ch->setEvents(kConnEventsRW);
      (void)loop_.updateChannel(ch);
    }
  }
}

void TcpServer::handle_write(int fd) {
  auto it = outbuf_.find(fd);
  if (it == outbuf_.end()) return;

  std::string& pending = it->second;
  std::string send_buf = pending;
  while (running_ && !pending.empty()) {
    ssize_t w = ::send(fd, pending.data(), pending.size(), MSG_NOSIGNAL);
    if (w > 0) {
      pending.erase(0, static_cast<size_t>(w));
      continue;
    }
    if (w < 0 && errno == EINTR) continue;
    if (w < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) break;
    close_conn(fd);
    return;
  }

  if (outbuf_.find(fd) != outbuf_.end() && pending.empty()) {
    if (!send_buf.empty()) {
      std::cout << "server send(fd=" << fd << "): " << send_buf << "\n";
    }
    auto it_ch = channels_.find(fd);
    if (it_ch != channels_.end()) {
      Channel* ch = it_ch->second;
      ch->setEvents(kConnEventsR);
      (void)loop_.updateChannel(ch);
    }
  }
}
