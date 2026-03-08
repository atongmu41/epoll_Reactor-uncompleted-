#pragma once

#include "public.h"

#include <functional>

// Reactor 模型中的最小 Channel 抽象：封装一个 fd 及其关心的事件和回调。
class Channel {
 public:
  using EventCallback = std::function<void()>;

  explicit Channel(int fd);
  ~Channel() = default;

  int fd() const { return fd_; }
  uint32_t events() const { return events_; }
  void setEvents(uint32_t ev) { events_ = ev; }

  void setReadCallback(EventCallback cb) { readCallback_ = std::move(cb); }
  void setWriteCallback(EventCallback cb) { writeCallback_ = std::move(cb); }
  void setErrorCallback(EventCallback cb) { errorCallback_ = std::move(cb); }
  void setCloseCallback(EventCallback cb) { closeCallback_ = std::move(cb); }

  // 根据 epoll 返回的 revents 分发到相应回调
  void handleEvent(uint32_t revents);

 private:
  int fd_;
  uint32_t events_{0};

  EventCallback readCallback_;
  EventCallback writeCallback_;
  EventCallback errorCallback_;
  EventCallback closeCallback_;
};

