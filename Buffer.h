#pragma once

#include "public.h"

// 简单的应用层缓冲区封装：
// - inputBuffer_：接收缓冲区，保存从 socket 读到但还未完全处理的数据
// - outputBuffer_：发送缓冲区，保存应用层待发送但 send() 尚未完全写出的数据
class Buffer {
public:
  Buffer() = default;
  ~Buffer() = default;

  // ==== 接收缓冲区相关 ====
  // 追加数据到接收缓冲区（通常在 recv() 后调用）
  void appendToInput(const char* data, size_t len) {
    if (data && len > 0) {
      inputBuffer_.append(data, len);
    }
  }

  // 读取接收缓冲区的全部内容（不清空）
  const std::string& input() const { return inputBuffer_; }

  // 清空接收缓冲区
  void clearInput() { inputBuffer_.clear(); }

  // ==== 发送缓冲区相关 ====
  // 追加数据到发送缓冲区（通常在需要发送一条完整消息时调用）
  void appendToOutput(const char* data, size_t len) {
    if (data && len > 0) {
      outputBuffer_.append(data, len);
    }
  }
  void appendToOutput(const std::string& s) {
    appendToOutput(s.data(), s.size());
  }

  // 获取当前发送缓冲区内容（用于 send()）
  const std::string& output() const { return outputBuffer_; }

  // 从发送缓冲区移除前 n 个字节（send() 成功写出的部分）
  void retrieveFromOutput(size_t n) {
    if (n >= outputBuffer_.size()) {
      outputBuffer_.clear();
    } else {
      outputBuffer_.erase(0, n);
    }
  }

  // 是否还有待发送的数据
  bool hasOutput() const { return !outputBuffer_.empty(); }

  // 清空发送缓冲区
  void clearOutput() { outputBuffer_.clear(); }

private:
  std::string inputBuffer_;
  std::string outputBuffer_;
};

