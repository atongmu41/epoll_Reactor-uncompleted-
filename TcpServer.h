#pragma once

#include "public.h"
#include "EventLoop.h"
#include "Acceptor.h"
#include "Connection.h"
#include "ThreadPool.h"

#include <memory>

// 组合 Acceptor（监听）+ 多条 Connection（连接）；只负责启动/停止与新连接分发。
class TcpServer {
 public:
  explicit TcpServer(uint16_t port = 5005, std::size_t workerCount = 2);
  ~TcpServer();

  TcpServer(const TcpServer&) = delete;
  TcpServer& operator=(const TcpServer&) = delete;

  bool start();
  void loop();
  void stop();

 private:
  void onNewConnection(int fd);
  void establishConnectionOnLoop(EventLoop* ioLoop, int fd);
  void removeConnection(Connection* conn);

  uint16_t port_;
  EventLoop loop_;
  Acceptor acceptor_;
  bool running_;
  std::unordered_map<int, std::unique_ptr<Connection>> connections_;
  std::size_t workerCount_;
  ThreadPool workerPool_;
  std::vector<std::unique_ptr<EventLoop>> workerLoops_;
  std::size_t nextLoopIndex_;
};
