#pragma once

#include "public.h"
#include "Socket.h"
#include "Epoll.h"

class EpollServer {
 public:
  explicit EpollServer(uint16_t port = 5005);
  ~EpollServer();

  EpollServer(const EpollServer&) = delete;
  EpollServer& operator=(const EpollServer&) = delete;

  // 创建监听 socket + epoll，并把监听 fd 加入 epoll
  bool start();

  // 阻塞事件循环；返回表示服务停止（错误或 stop()）
  void loop();

  // 主动停止服务（可安全重复调用）
  void stop();

 private:
  void close_conn(int fd);
  void handle_accept();
  void handle_read(int fd);
  void handle_write(int fd);

  uint16_t port_;
  Socket listen_sock_;
  Epoll epoll_;
  bool running_;

  std::unordered_map<int, std::string> outbuf_;
  std::vector<epoll_event> events_;
};
