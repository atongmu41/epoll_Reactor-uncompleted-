#include "EventLoop.h"
#include "Connection.h"

#include <sys/eventfd.h>
#include <sys/timerfd.h>

EventLoop::EventLoop()
    : epoll_(), events_(1024), running_(false), wakeupFd_(-1), timerFd_(-1) {}

EventLoop::~EventLoop() { close(); }

bool EventLoop::addChannel(Channel* ch) {
  if (!ch) return false;
  if (!epoll_.valid() && !epoll_.create()) return false;
  return epoll_.add(ch);
}

bool EventLoop::updateChannel(Channel* ch) {
  if (!ch || !epoll_.valid()) return false;
  return epoll_.mod(ch);
}

bool EventLoop::removeChannel(Channel* ch) {
  if (!ch || !epoll_.valid()) return false;
  return epoll_.del(ch);
}

void EventLoop::runInLoop(std::function<void()> cb) {
  if (isInLoopThread()) {
    if (cb) cb();
    return;
  }
  queueInLoop(std::move(cb));
}

void EventLoop::queueInLoop(std::function<void()> cb) {
  if (!cb) return;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    pendingFunctors_.push_back(std::move(cb));
  }
  if (!isInLoopThread() && wakeupFd_ >= 0 && running_) {
    uint64_t one = 1;
    ssize_t n = ::write(wakeupFd_, &one, sizeof(one));
    (void)n;
  }
}

void EventLoop::doPendingFunctors() {
  std::vector<std::function<void()>> functors;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    functors.swap(pendingFunctors_);
  }
  for (const auto& f : functors) {
    if (f) f();
  }
}

void EventLoop::handleWakeup() {
  uint64_t buf = 0;
  ssize_t n = ::read(wakeupFd_, &buf, sizeof(buf));
  (void)n;
  doPendingFunctors();
}

void EventLoop::registerConnection(int fd, Connection* conn) {
  connectionMap_[fd] = conn;
}

void EventLoop::unregisterConnection(int fd) {
  connectionMap_.erase(fd);
}

void EventLoop::setupIdleTimer() {
  if (timerFd_ >= 0) return;
  timerFd_ = ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
  if (timerFd_ < 0) {
    std::cerr << "EventLoop timerfd_create failed: " << std::strerror(errno) << "\n";
    return;
  }
  itimerspec its{};
  its.it_interval.tv_sec = kIdleTimeoutSec;
  its.it_value.tv_sec = kIdleTimeoutSec;
  if (::timerfd_settime(timerFd_, 0, &its, nullptr) < 0) {
    std::cerr << "EventLoop timerfd_settime failed: " << std::strerror(errno) << "\n";
    ::close(timerFd_);
    timerFd_ = -1;
    return;
  }
  timerChannel_.reset(new Channel(timerFd_));
  timerChannel_->setEvents(EPOLLIN | EPOLLERR);
  timerChannel_->setReadCallback([this]() { onIdleTimer(); });
  if (!epoll_.add(timerChannel_.get())) {
    std::cerr << "EventLoop add timer channel failed\n";
    timerChannel_.reset();
    ::close(timerFd_);
    timerFd_ = -1;
  }
}

void EventLoop::onIdleTimer() {
  if (timerFd_ < 0) return;
  uint64_t exp = 0;
  ssize_t n = ::read(timerFd_, &exp, sizeof(exp));
  (void)n;
  for (auto& kv : connectionMap_) {
    Connection* conn = kv.second;
    if (conn && conn->isIdleFor(std::chrono::seconds(kIdleTimeoutSec))) {
      conn->forceClose();
    }
  }
}

void EventLoop::run() {
  running_ = true;
  threadId_ = std::this_thread::get_id();
  if (!epoll_.valid()) {
    if (!epoll_.create()) {
      std::cerr << "EventLoop create epoll failed: " << std::strerror(errno) << "\n";
      running_ = false;
      return;
    }
  }
  if (wakeupFd_ < 0) {
    wakeupFd_ = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (wakeupFd_ < 0) {
      std::cerr << "EventLoop eventfd failed: " << std::strerror(errno) << "\n";
      running_ = false;
      return;
    }
    wakeupChannel_.reset(new Channel(wakeupFd_));
    wakeupChannel_->setEvents(EPOLLIN);
    wakeupChannel_->setReadCallback([this]() { handleWakeup(); });
    if (!epoll_.add(wakeupChannel_.get())) {
      std::cerr << "EventLoop add wakeup channel failed\n";
      ::close(wakeupFd_);
      wakeupFd_ = -1;
      wakeupChannel_.reset();
      running_ = false;
      return;
    }
  }
  setupIdleTimer();
  while (running_) {
    int n = epoll_.wait(events_, -1);
    if (n < 0) {
      int err = errno;
      if (err == EINTR) continue;
      if (err == EBADF && !running_) break;
      std::cerr << "epoll_wait failed: " << std::strerror(err) << "\n";
      break;
    }
    for (int i = 0; i < n; i++) {
      Channel* ch = static_cast<Channel*>(events_[i].data.ptr);
      if (!ch) continue;
      ch->handleEvent(events_[i].events);
    }
    doPendingFunctors();
  }
  running_ = false;
}

void EventLoop::quit() { running_ = false; }

void EventLoop::close() {
  running_ = false;
  if (wakeupChannel_ && epoll_.valid()) {
    epoll_.del(wakeupChannel_.get());
  }
  wakeupChannel_.reset();
  if (wakeupFd_ >= 0) {
    ::close(wakeupFd_);
    wakeupFd_ = -1;
  }
  if (timerChannel_ && epoll_.valid()) {
    epoll_.del(timerChannel_.get());
  }
  timerChannel_.reset();
  if (timerFd_ >= 0) {
    ::close(timerFd_);
    timerFd_ = -1;
  }
  epoll_.close();
}
