#include "Connection.h"

static constexpr uint32_t kConnEventsR =
    EPOLLIN | EPOLLRDHUP | EPOLLERR;
static constexpr uint32_t kConnEventsRW =
    EPOLLIN | EPOLLOUT | EPOLLRDHUP | EPOLLERR;

Connection::Connection(EventLoop* loop, int fd)
    : loop_(loop), fd_(fd), channel_(nullptr),
      established_at_(std::chrono::steady_clock::now()) {}

Connection::~Connection() {
  if (channel_ && loop_ && loop_->valid()) {
    loop_->removeChannel(channel_);
  }
  if (channel_) {
    delete channel_;
    channel_ = nullptr;
  }
  if (fd_ >= 0) {
    ::close(fd_);
    fd_ = -1;
  }
}

bool Connection::establish() {
  if (!loop_ || fd_ < 0) return false;
  channel_ = new Channel(fd_);
  channel_->setEvents(kConnEventsR);
  channel_->setReadCallback(std::bind(&Connection::handleRead, this));
  channel_->setWriteCallback(std::bind(&Connection::handleWrite, this));
  channel_->setErrorCallback(std::bind(&Connection::handleClose, this));
  channel_->setCloseCallback(std::bind(&Connection::handleClose, this));

  if (!loop_->addChannel(channel_)) {
    delete channel_;
    channel_ = nullptr;
    return false;
  }
  established_at_ = std::chrono::steady_clock::now();
  return true;
}

void Connection::handleRead() {
  if (!channel_ || fd_ < 0) return;
  char buf[4096];
  std::string recv_buf;
  while (true) {
    ssize_t r = ::recv(fd_, buf, sizeof(buf), 0);
    if (r > 0) {
      outbuf_.append(buf, static_cast<size_t>(r));
      recv_buf.append(buf, static_cast<size_t>(r));
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

  if (!recv_buf.empty()) {
    ++message_count_;
  }

  if (!outbuf_.empty() && channel_) {
    channel_->setEvents(kConnEventsRW);
    (void)loop_->updateChannel(channel_);
  }
}

void Connection::handleWrite() {
  if (!channel_ || fd_ < 0) return;
  std::string send_buf = outbuf_;
  while (!outbuf_.empty()) {
    ssize_t w = ::send(fd_, outbuf_.data(), outbuf_.size(), MSG_NOSIGNAL);
    if (w > 0) {
      outbuf_.erase(0, static_cast<size_t>(w));
      continue;
    }
    if (w < 0 && errno == EINTR) continue;
    if (w < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) break;
    handleClose();
    return;
  }

  if (outbuf_.empty() && channel_) {
    channel_->setEvents(kConnEventsR);
    (void)loop_->updateChannel(channel_);
  }
}

void Connection::handleClose() {
  if (!channel_) return;
  if (message_count_ > 0) {
    auto dur = std::chrono::steady_clock::now() - established_at_;
    double sec = std::chrono::duration<double>(dur).count();
    std::cout << "连接 fd=" << fd_ << " 关闭，共处理 " << message_count_ << " 条，耗时 " << sec << " 秒\n";
  }
  if (loop_ && loop_->valid()) {
    loop_->removeChannel(channel_);
  }
  CloseCallback cb = closeCallback_;
  closeCallback_ = nullptr;
  if (cb) {
    cb(this);  // 回调中 TcpServer 会 erase 并 delete this，析构函数负责 close(fd_) 和 delete channel_
  }
}
