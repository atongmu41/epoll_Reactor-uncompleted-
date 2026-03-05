#pragma once

#include "public.h"

// 对 IPv4 地址的简单封装：负责 ip/端口的获取与网络/主机字节序转换。
class InetAddress {
 public:
  InetAddress();
  InetAddress(const std::string& ip, uint16_t port);  // ip 形如 "127.0.0.1"
  explicit InetAddress(uint16_t port);                // 0.0.0.0:port

  // 从现有 sockaddr_in 构造
  explicit InetAddress(const sockaddr_in& addr);

  // 返回内部 sockaddr_in（网络字节序）
  const sockaddr_in& getSockAddr() const { return addr_; }
  sockaddr_in* getSockAddrPtr() { return &addr_; }

  // 主机字节序的 ip/port
  std::string ip() const;
  uint16_t port() const;

 private:
  sockaddr_in addr_;
};
