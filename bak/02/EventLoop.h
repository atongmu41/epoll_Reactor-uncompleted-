#pragma once

#include "public.h"
#include "Epoll.h"
#include "Channel.h"

// 事件循环：持有 Epoll，执行 epoll_wait 并将就绪事件分发给对应 Channel。
class EventLoop {
 public:
  EventLoop();
  ~EventLoop();

  EventLoop(const EventLoop&) = delete;
  EventLoop& operator=(const EventLoop&) = delete;

  // 注册 Channel 到 epoll（首次调用时创建 epoll 实例）
  bool addChannel(Channel* ch);
  // 更新 Channel 关注的事件
  bool updateChannel(Channel* ch);
  // 从 epoll 中移除 Channel
  bool removeChannel(Channel* ch);

  // 阻塞运行事件循环，直到 quit() 被调用或出错
  void run();
  // 退出事件循环（下一轮 while 检查时退出）
  void quit();
  // 关闭 epoll fd（stop 时调用）
  void close();

  bool isRunning() const { return running_; }
  bool valid() const { return epoll_.valid(); }

 private:
  Epoll epoll_;
  std::vector<epoll_event> events_;
  bool running_;
};
