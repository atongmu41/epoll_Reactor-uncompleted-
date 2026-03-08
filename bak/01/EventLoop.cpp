#include "EventLoop.h"

EventLoop::EventLoop() : epoll_(), events_(1024), running_(false) {}

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

void EventLoop::run() {
  running_ = true;
  while (running_) {
    int n = epoll_.wait(events_, -1);
    if (n < 0) {
      if (errno == EINTR) continue;
      std::cerr << "epoll_wait failed: " << std::strerror(errno) << "\n";
      break;
    }
    for (int i = 0; i < n; i++) {
      Channel* ch = static_cast<Channel*>(events_[i].data.ptr);
      if (!ch) continue;
      ch->handleEvent(events_[i].events);
    }
  }
  running_ = false;
}

void EventLoop::quit() { running_ = false; }

void EventLoop::close() {
  running_ = false;
  epoll_.close();
}
