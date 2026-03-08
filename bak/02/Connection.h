#pragma once

#include "public.h"
#include "EventLoop.h"
#include "Channel.h"

#include <chrono>
#include <functional>

// 封装单条客户端连接：fd + Channel + 写缓冲，读/写/关闭由内部 Channel 回调处理。
class Connection {
 public:
  using CloseCallback = std::function<void(Connection*)>;

  Connection(EventLoop* loop, int fd);
  ~Connection();

  Connection(const Connection&) = delete;
  Connection& operator=(const Connection&) = delete;

  // 注册到 EventLoop 并设置读/写/关闭回调
  bool establish();
  void setCloseCallback(CloseCallback cb) { closeCallback_ = std::move(cb); }

  int fd() const { return fd_; }
  Channel* channel() { return channel_; }

  // 当前连接已处理的消息条数（每完整收+发一轮计 1 条）
  int messageCount() const { return message_count_; }

 private:
  void handleRead();
  void handleWrite();
  void handleClose();

  EventLoop* loop_;
  int fd_;
  Channel* channel_;
  std::string outbuf_;
  CloseCallback closeCallback_;
  int message_count_{0};
  std::chrono::steady_clock::time_point established_at_;
};
