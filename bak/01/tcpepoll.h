#pragma once

#include "TcpServer.h"

// 原 EpollServer 逻辑已封装到 TcpServer，此处保留别名便于兼容。
using EpollServer = TcpServer;
