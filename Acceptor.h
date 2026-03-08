#pragma once

#include "public.h"
#include "Socket.h"
#include "EventLoop.h"
#include "Channel.h"
#include "InetAddress.h"

#include <functional>

// 封装“监听”职责：监听 Socket + 监听 Channel，有连接时通过回调上报。
class Acceptor {
 public:
  using NewConnectionCallback = std::function<void(int fd)>;

  Acceptor();
  ~Acceptor();

  Acceptor(const Acceptor&) = delete;
  Acceptor& operator=(const Acceptor&) = delete;

  // 在 loop 上监听 port；有连接时调用 newConnectionCallback(fd)
  bool start(EventLoop* loop, uint16_t port);
  void stop();

  void setNewConnectionCallback(NewConnectionCallback cb) {
    newConnectionCallback_ = std::move(cb);
  }

  bool listening() const { return listen_channel_ != nullptr; }

 private:
  void handleAccept();

  Socket listen_sock_;
  Channel* listen_channel_{nullptr};
  EventLoop* loop_{nullptr};
  uint16_t port_{0};
  NewConnectionCallback newConnectionCallback_;
};
