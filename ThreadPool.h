#pragma once

#include "public.h"

#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

// 简单的固定大小线程池实现：
// - 通过 submit() 提交 std::function<void()> 任务
// - 内部有若干工作线程从任务队列中取出任务并执行
class ThreadPool {
 public:
  explicit ThreadPool(std::size_t threadCount = std::thread::hardware_concurrency());
  ~ThreadPool();

  ThreadPool(const ThreadPool&) = delete;
  ThreadPool& operator=(const ThreadPool&) = delete;

  // 显式启动线程池（若未启动）。通常构造后即可调用，也可延迟到需要时再调。
  void start();

  // 停止线程池：
  // - 不再接受新任务
  // - 等待队列中已有任务被工作线程取走并执行完
  // - 然后回收所有工作线程
  void stop();

  // 提交一个任务到线程池执行。
  // 任务类型要求可以转换为 std::function<void()>。
  template <typename F>
  void submit(F&& f) {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (!running_) {
        // 若尚未 start() 就提交任务，这里选择自动启动线程池。
        startUnlocked();
      }
      tasks_.emplace(std::forward<F>(f));
    }
    cv_.notify_one();
  }

 private:
  void workerLoop();

  // 不加锁版本，仅供内部在持有 mutex_ 时调用。
  void startUnlocked();

  std::size_t threadCount_;
  std::vector<std::thread> workers_;
  std::queue<std::function<void()>> tasks_;

  std::mutex mutex_;
  std::condition_variable cv_;
  bool running_;
};

