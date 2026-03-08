#pragma once

class Channel;

#include "public.h"

// 对 epoll 的简单封装：负责 epoll_create / epoll_ctl / epoll_wait。
class Epoll {
 public:
  Epoll();
  ~Epoll();

  Epoll(const Epoll&) = delete;
  Epoll& operator=(const Epoll&) = delete;

  // 创建 epoll 实例
  bool create();

  // 关闭 epoll fd
  void close();

  int fd() const { return epfd_; }
  bool valid() const { return epfd_ >= 0; }

  bool add(int fd, uint32_t events);
  bool mod(int fd, uint32_t events);
  bool del(int fd);

  // 使用 data.ptr 直接绑定 Channel*
  bool add(Channel* ch);
  bool mod(Channel* ch);
  bool del(Channel* ch);

  // 等待事件，返回就绪事件数量，<0 表示出错（errno 保留）
  int wait(std::vector<epoll_event>& events, int timeout_ms);

 private:
  int epfd_;
};

