#pragma once

#include "public.h"
#include "Socket.h"
#include "EventLoop.h"
#include "Channel.h"
#include "InetAddress.h"

// 封装：监听 Socket、监听/连接 Channel、EventLoop 注册，以及 accept/读/写/关连接回调。
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
  void close_conn(int fd);
  void handle_accept();
  void handle_read(int fd);
  void handle_write(int fd);

  uint16_t port_;
  Socket listen_sock_;
  EventLoop loop_;
  bool running_;

  std::unordered_map<int, std::string> outbuf_;
  Channel* listen_channel_{nullptr};
  std::unordered_map<int, Channel*> channels_;
};
