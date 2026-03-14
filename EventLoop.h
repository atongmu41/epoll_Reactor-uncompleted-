#pragma once

#include "public.h"
#include "Epoll.h"
#include "Channel.h"

#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

class Connection;

// 事件循环：持有 Epoll，执行 epoll_wait 并将就绪事件分发给对应 Channel。
// 支持 runInLoop/queueInLoop 跨线程投递任务，通过 wakeup fd 唤醒 epoll_wait。
// 持有本 loop 上的 Connection 登记表，定时检查空闲并关闭。
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

  // 若在当前 loop 线程则立即执行 cb，否则投递到队列并写 wakeup fd 唤醒
  void runInLoop(std::function<void()> cb);
  // 将 cb 放入待执行队列；若当前不在 loop 线程则写 wakeup fd 唤醒
  void queueInLoop(std::function<void()> cb);
  // 当前线程是否为该 EventLoop 所属线程（仅在 run() 被调用后有效）
  bool isInLoopThread() const { return std::this_thread::get_id() == threadId_; }

  // 阻塞运行事件循环，直到 quit() 被调用或出错
  void run();
  // 退出事件循环（下一轮 while 检查时退出）
  void quit();
  // 关闭 epoll 与 wakeup fd（stop 时调用）
  void close();

  bool isRunning() const { return running_; }
  bool valid() const { return epoll_.valid(); }

  // 建立连接后在该 loop 线程调用，将 conn 登记到本 loop
  void registerConnection(int fd, Connection* conn);
  // 关闭连接时在 loop 线程调用，从登记表移除
  void unregisterConnection(int fd);

 private:
  void doPendingFunctors();
  void handleWakeup();
  void setupIdleTimer();
  void onIdleTimer();

  static constexpr int kIdleTimeoutSec = 5;

  Epoll epoll_;
  std::vector<epoll_event> events_;
  bool running_;
  int wakeupFd_;
  std::unique_ptr<Channel> wakeupChannel_;
  std::vector<std::function<void()>> pendingFunctors_;
  std::mutex mutex_;
  std::thread::id threadId_;
  std::unordered_map<int, Connection*> connectionMap_;
  int timerFd_;
  std::unique_ptr<Channel> timerChannel_;
};
