#pragma once

#include "public.h"
#include "EventLoop.h"
#include "Acceptor.h"
#include "Connection.h"

// 组合 Acceptor（监听）+ 多条 Connection（连接）；只负责启动/停止与新连接分发。
class TcpServer {
 public:
  explicit TcpServer(uint16_t port = 5005);
  ~TcpServer();

  TcpServer(const TcpServer&) = delete;
  TcpServer& operator=(const TcpServer&) = delete;

  bool start();
  void loop();
  void stop();

 private:
  void onNewConnection(int fd);
  void removeConnection(Connection* conn);

  uint16_t port_;
  EventLoop loop_;
  Acceptor acceptor_;
  bool running_;
  std::unordered_map<int, Connection*> connections_;
};
