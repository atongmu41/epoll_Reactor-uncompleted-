#include "tcpepoll.h"

// EPOLLIN     : fd 可读（有数据可读 / 有新连接可 accept）
// EPOLLOUT    : fd 可写（发送缓冲区有空间，可继续 send；常用于处理短写）
// EPOLLERR    : fd 发生错误（需要尽快 close；错误细节可通过 getsockopt(SO_ERROR) 查询）
// EPOLLRDHUP  : 对端关闭了读端（半关闭/对端不再发送；recv 返回 0 前可能先收到该事件）
static constexpr uint32_t kListenEvents = EPOLLIN | EPOLLERR;  // 监听 fd：只关心新连接 + 错误
static constexpr uint32_t kConnEventsR =
    EPOLLIN | EPOLLRDHUP | EPOLLERR;  // 连接 fd：读 + 对端关闭/错误
static constexpr uint32_t kConnEventsRW =
    EPOLLIN | EPOLLOUT | EPOLLRDHUP | EPOLLERR;  // 连接 fd：读写 + 对端关闭/错误

EpollServer::EpollServer(uint16_t port)
    : port_(port), listen_sock_(), epoll_(), running_(false), events_(1024) {}

EpollServer::~EpollServer() { stop(); }

bool EpollServer::start() {
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

  InetAddress listen_addr(port_);  // 0.0.0.0:port_
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

  if (!epoll_.create()) {
    std::cerr << "epoll_create1 failed: " << std::strerror(errno) << "\n";
    stop();
    return false;
  }

  if (!epoll_.add(listen_sock_.fd(), kListenEvents)) {
    std::cerr << "epoll_ctl ADD listen_fd failed: " << std::strerror(errno) << "\n";
    stop();
    return false;
  }

  running_ = true;
  std::cout << "epoll echo server listening on 0.0.0.0:" << port_ << "\n";
  return true;
}

void EpollServer::stop() {
  running_ = false;

  if (epoll_.valid()) {
    for (auto& kv : outbuf_) {
      epoll_.del(kv.first);
      ::close(kv.first);
    }
    outbuf_.clear();

    if (listen_sock_.valid()) {
      epoll_.del(listen_sock_.fd());
    }
    epoll_.close();
  }

  listen_sock_.close();
}

void EpollServer::loop() {
  if (!running_) {
    if (!start()) return;
  }

  while (running_) {
    int n = epoll_.wait(events_, -1);
    if (n < 0) {
      if (errno == EINTR) continue;
      std::cerr << "epoll_wait failed: " << std::strerror(errno) << "\n";
      break;
    }

    for (int i = 0; i < n; i++) {
      int fd = events_[i].data.fd;
      uint32_t ev = events_[i].events;

      if (fd == listen_sock_.fd()) {
        if (ev & EPOLLERR) {
          std::cerr << "listen_fd EPOLLERR\n";
          continue;
        }
        handle_accept();
        continue;
      }

      if (ev & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
        close_conn(fd);
        continue;
      }

      if (ev & EPOLLIN) handle_read(fd);
      if (ev & EPOLLOUT) handle_write(fd);
    }
  }

  stop();
}

void EpollServer::close_conn(int fd) {
  if (epoll_.valid()) epoll_.del(fd);
  ::close(fd);
  outbuf_.erase(fd);
}

void EpollServer::handle_accept() {
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

    if (!epoll_.add(cfd, kConnEventsR)) {
      std::cerr << "epoll_ctl ADD conn failed: " << std::strerror(errno) << "\n";
      ::close(cfd);
      continue;
    }
    outbuf_.emplace(cfd, std::string{});
  }
}

void EpollServer::handle_read(int fd) {
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
    (void)epoll_.mod(fd, kConnEventsRW);
  }
}

void EpollServer::handle_write(int fd) {
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
    (void)epoll_.mod(fd, kConnEventsR);
  }
}

int main(int argc, char* argv[]) {
  int port = 5005;
  if (argc >= 2) {
    port = std::atoi(argv[1]);
    if (port <= 0 || port > 65535) {
      std::cerr << "invalid port: " << argv[1] << "\n";
      return -1;
    }
  }

  EpollServer server(static_cast<uint16_t>(port));
  if (!server.start()) return -1;
  server.loop();
  return 0;
}
