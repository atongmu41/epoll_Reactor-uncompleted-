#include "ThreadPool.h"

ThreadPool::ThreadPool(std::size_t threadCount)
    : threadCount_(threadCount ? threadCount : 1), running_(false) {}

ThreadPool::~ThreadPool() { stop(); }

void ThreadPool::start() {
  std::lock_guard<std::mutex> lock(mutex_);
  startUnlocked();
}

void ThreadPool::startUnlocked() {
  if (running_) return;
  running_ = true;
  // 避免重复创建线程
  if (!workers_.empty()) return;

  workers_.reserve(threadCount_);
  for (std::size_t i = 0; i < threadCount_; ++i) {
    workers_.emplace_back(&ThreadPool::workerLoop, this);
  }
}

void ThreadPool::stop() {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!running_) return;
    running_ = false;
  }
  cv_.notify_all();

  for (std::thread& t : workers_) {
    if (t.joinable()) {
      t.join();
    }
  }
  workers_.clear();

  // 理论上此时 tasks_ 应该已被工作线程取空，保险起见可清空队列
  {
    std::lock_guard<std::mutex> lock(mutex_);
    while (!tasks_.empty()) tasks_.pop();
  }
}

void ThreadPool::workerLoop() {
  for (;;) {
    std::function<void()> task;
    {
      std::unique_lock<std::mutex> lock(mutex_);
      // 等待任务或停止信号
      cv_.wait(lock, [this] { return !running_ || !tasks_.empty(); });

      // 若线程池已停止且没有任务，退出循环
      if (!running_ && tasks_.empty()) {
        return;
      }

      // 取出一个任务执行
      if (!tasks_.empty()) {
        task = std::move(tasks_.front());
        tasks_.pop();
      }
    }

    if (task) {
      task();
    }
  }
}

