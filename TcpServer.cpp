#include "TcpServer.h"

#include <functional>

TcpServer::TcpServer(uint16_t port, std::size_t workerCount)
    : port_(port),
      loop_(),
      acceptor_(),
      running_(false),
      workerCount_(workerCount ? workerCount : 1),
      workerPool_(workerCount ? workerCount : 1),
      workerLoops_(),
      nextLoopIndex_(0) {
  workerLoops_.reserve(workerCount_);
  for (std::size_t i = 0; i < workerCount_; ++i) {
    workerLoops_.emplace_back(new EventLoop());
  }
}

TcpServer::~TcpServer() { stop(); }

bool TcpServer::start() {
  if (running_) return true;

  ::signal(SIGPIPE, SIG_IGN);

  // 启动从 Reactor 线程：每个线程运行一个独立的 EventLoop::run()
  for (const auto& p : workerLoops_) {
    EventLoop* loop = p.get();
    workerPool_.submit([loop]() {
      loop->run();
    });
  }

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
      Connection* c = it->second.get();
      ++it;
      removeConnection(c);
    }
    acceptor_.stop();
    loop_.close();
  }

  // 通知从 EventLoop 退出并关闭其 epoll
  for (const auto& p : workerLoops_) {
    if (p) {
      p->quit();
      p->close();
    }
  }
  // 停止工作线程并回收
  workerPool_.stop();
  workerLoops_.clear();
}

void TcpServer::loop() {
  if (!running_) {
    if (!start()) return;
  }
  loop_.run();
  stop();
}

void TcpServer::onNewConnection(int fd) {
  EventLoop* ioLoop = &loop_;
  if (!workerLoops_.empty()) {
    ioLoop = workerLoops_[nextLoopIndex_].get();
    nextLoopIndex_ = (nextLoopIndex_ + 1) % workerLoops_.size();
  }
  ioLoop->runInLoop(std::bind(&TcpServer::establishConnectionOnLoop, this, ioLoop, fd));
}

void TcpServer::establishConnectionOnLoop(EventLoop* ioLoop, int fd) {
  std::unique_ptr<Connection> conn(new Connection(ioLoop, fd));
  conn->setCloseCallback(
      std::bind(&TcpServer::removeConnection, this, std::placeholders::_1));

  if (!conn->establish()) {
    std::cerr << "Connection establish failed for fd=" << fd << "\n";
    ::close(fd);
    return;
  }
  connections_[fd] = std::move(conn);
  ioLoop->registerConnection(fd, connections_[fd].get());
}

void TcpServer::removeConnection(Connection* conn) {
  if (!conn) return;
  int fd = conn->fd();
  connections_.erase(fd);
}
