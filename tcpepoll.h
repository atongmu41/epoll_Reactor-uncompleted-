#pragma once

#include "public.h"
#include "Socket.h"
#include "Epoll.h"
#include "Channel.h"

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

  // 每个连接 fd -> 待发送数据（回显缓冲），用于处理短写与排队发送
  std::unordered_map<int, std::string> outbuf_;
  // epoll_wait 的就绪事件数组，由 Epoll::wait 填充
  std::vector<epoll_event> events_;

  // 监听 socket 对应的 Channel（仅一个），data.ptr 指向它以便事件分发
  Channel* listen_channel_{nullptr};
  // 每个连接 fd -> 其 Channel*，用于 handle_read/handle_write 中 mod 事件、close_conn 中删除并释放
  std::unordered_map<int, Channel*> channels_;
};
