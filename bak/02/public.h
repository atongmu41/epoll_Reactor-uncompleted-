#pragma once

// 说明：
// - 这里放“项目里高频使用”的通用 include / 小工具，便于快速开发。
// - 但仍建议：头文件（比如 epoll.h）尽量只依赖它真正需要暴露的类型。

// C / POSIX
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

// C++
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>
