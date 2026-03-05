#pragma once

#include "public.h"

struct ClientOptions {
  std::string host = "127.0.0.1";
  uint16_t port = 5005;
  int connections = 1000;
  int duration_sec = 10;
  int message_size = 64;
};

// 返回 0 表示正常完成；非 0 表示参数/运行错误
int run_client(const ClientOptions& opt);
