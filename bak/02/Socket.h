#pragma once

#include "public.h"
#include "InetAddress.h"

// 对 TCP socket 的简单封装，负责 socket/bind/listen 等基础操作。
class Socket {
 public:
  Socket();
  explicit Socket(int fd);
  ~Socket();

  Socket(const Socket&) = delete;
  Socket& operator=(const Socket&) = delete;

  int fd() const { return fd_; }
  bool valid() const { return fd_ >= 0; }

  // 创建 AF_INET/SOCK_STREAM TCP socket
  bool create();

  // SO_REUSEADDR
  bool setReuseAddr(bool on);

  // 非阻塞
  bool setNonBlock();

  // 绑定地址（网络字节序）
  bool bind(const InetAddress& addr);

  // 监听
  bool listen(int backlog = SOMAXCONN) const;

  // 关闭 fd
  void close();

 private:
  int fd_;
};

