#include "Connection.h"

static constexpr uint32_t kConnEventsR =
    EPOLLIN | EPOLLRDHUP | EPOLLERR | EPOLLET;
static constexpr uint32_t kConnEventsRW =
    EPOLLIN | EPOLLOUT | EPOLLRDHUP | EPOLLERR | EPOLLET;

Connection::Connection(EventLoop* loop, int fd)
    : loop_(loop), fd_(fd) {
  auto now = std::chrono::steady_clock::now();
  established_at_ = now;
  lastActive_ = now;
}

Connection::~Connection() {
  if (channel_ && loop_ && loop_->valid()) {
    loop_->removeChannel(channel_.get());
  }
  if (fd_ >= 0) {
    ::close(fd_);
    fd_ = -1;
  }
}

bool Connection::establish() {
  if (!loop_ || fd_ < 0) return false;
  channel_.reset(new Channel(fd_));
  channel_->setEvents(kConnEventsR);
  channel_->setReadCallback(std::bind(&Connection::handleRead, this));
  channel_->setWriteCallback(std::bind(&Connection::handleWrite, this));
  channel_->setErrorCallback(std::bind(&Connection::handleClose, this));
  channel_->setCloseCallback(std::bind(&Connection::handleClose, this));

  if (!loop_->addChannel(channel_.get())) {
    channel_.reset();
    return false;
  }
  auto now = std::chrono::steady_clock::now();
  established_at_ = now;
  lastActive_ = now;
  return true;
}

void Connection::handleRead() {
  if (!channel_ || fd_ < 0) return;
  char buf[4096];
  while (true) {
    ssize_t r = ::recv(fd_, buf, sizeof(buf), 0);
    if (r > 0) {
      buffer_.appendToInput(buf, static_cast<size_t>(r));
      buffer_.appendToOutput(buf, static_cast<size_t>(r));  // 回显：读多少就写回多少
      lastActive_ = std::chrono::steady_clock::now();
      continue;
    }
    if (r == 0) {
      handleClose();
      return;
    }
    if (errno == EINTR) continue;
    if (errno == EAGAIN || errno == EWOULDBLOCK) break;
    handleClose();
    return;
  }

  if (!buffer_.input().empty()) {
    ++message_count_;
  }

  if (buffer_.hasOutput() && channel_) {
    channel_->setEvents(kConnEventsRW);
    (void)loop_->updateChannel(channel_.get());
  }
}

void Connection::handleWrite() {
  if (!channel_ || fd_ < 0) return;
  while (buffer_.hasOutput()) {
    const std::string& out = buffer_.output();
    ssize_t w = ::send(fd_, out.data(), out.size(), MSG_NOSIGNAL);
    if (w > 0) {
      buffer_.retrieveFromOutput(static_cast<size_t>(w));
      lastActive_ = std::chrono::steady_clock::now();
      continue;
    }
    if (w < 0 && errno == EINTR) continue;
    if (w < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) break;
    handleClose();
    return;
  }

  if (!buffer_.hasOutput() && channel_) {
    channel_->setEvents(kConnEventsR);
    (void)loop_->updateChannel(channel_.get());
  }
}

void Connection::refreshAlive() {
  lastActive_ = std::chrono::steady_clock::now();
}

bool Connection::isIdleFor(const std::chrono::seconds& timeout) const {
  auto now = std::chrono::steady_clock::now();
  return (now - lastActive_) >= timeout;
}

void Connection::forceClose() {
  if (!loop_) return;
  // 必须用 queueInLoop：若从 onIdleTimer 的 connectionMap_ 遍历中调用，handleClose 会 erase 当前迭代器，导致未定义行为
  loop_->queueInLoop(std::bind(&Connection::handleClose, this));
}

void Connection::handleClose() {
  if (!channel_) return;
  if (message_count_ > 0) {
    auto dur = std::chrono::steady_clock::now() - established_at_;
    double sec = std::chrono::duration<double>(dur).count();
    std::cout << "连接 fd=" << fd_ << " 关闭，共处理 " << message_count_ << " 条，耗时 " << sec << " 秒\n";
  }
  if (loop_ && loop_->valid()) {
    loop_->unregisterConnection(fd_);
    loop_->removeChannel(channel_.get());
    channel_.reset();  // 析构时不再对已移除的 channel 做 removeChannel
  }
  // 不保留局部 std::function，避免 handleClose 返回时析构导致崩溃；将 closeCallback_ 直接 move 进队列
  if (closeCallback_) {
    loop_->queueInLoop(std::bind(std::move(closeCallback_), this));
    closeCallback_ = nullptr;
  }
}
