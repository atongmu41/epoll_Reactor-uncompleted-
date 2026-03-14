# 基于 epoll + Reactor 的高并发 TCP 服务器（C++11）

基于 Linux epoll 与 Reactor 模型的 TCP 回显服务端与测试客户端，采用主从 Reactor、线程池、边缘触发与空闲连接超时关闭。

## 项目结构

### 入口与服务

| 文件 | 说明 |
|------|------|
| `tcpepoll.cpp` | 服务端入口：解析端口，构造 `TcpServer` 并启动事件循环 |
| `tcpepoll.h` | 提供 `EpollServer` 类型别名（即 `TcpServer`） |
| `TcpServer.h` / `TcpServer.cpp` | 组合 Acceptor 与多条 Connection，主从 Reactor：主线程负责 accept，从线程池中的 worker EventLoop 负责连接 IO；新连接按轮询分配到 worker |

### 事件循环与网络 IO

| 文件 | 说明 |
|------|------|
| `EventLoop.h` / `EventLoop.cpp` | 事件循环：持有 Epoll，执行 `epoll_wait` 并分发到 Channel；支持 `runInLoop` / `queueInLoop`（通过 eventfd 唤醒）；维护本 loop 上的 Connection 登记表与 5 秒空闲定时器（timerfd） |
| `Epoll.h` / `Epoll.cpp` | 对 `epoll_create` / `epoll_ctl` / `epoll_wait` 的封装，支持按 fd 或按 `Channel*`（`data.ptr`）注册 |
| `Channel.h` / `Channel.cpp` | 与单个 fd 绑定的事件分发单元：读/写/错误/关闭回调，`handleEvent(revents)` 按事件调用 |

### 连接与监听

| 文件 | 说明 |
|------|------|
| `Acceptor.h` / `Acceptor.cpp` | 监听 socket：创建、bind、listen，为 listen fd 建 Channel 并注册到主 EventLoop，在回调中 `accept4` 并回调 `onNewConnection` |
| `Connection.h` / `Connection.cpp` | 单条客户端连接：fd + Channel + Buffer；读/写/关闭由 Channel 回调处理；支持 `forceClose`、空闲检测（`isIdleFor`）；连接 Channel 使用 **EPOLLET** 避免 workloop 空转 |
| `Buffer.h` / `Buffer.cpp` | 应用层缓冲：接收缓冲区（input）与发送缓冲区（output） |

### 基础设施

| 文件 | 说明 |
|------|------|
| `Socket.h` / `Socket.cpp` | TCP socket 封装：create、setReuseAddr、bind、listen、close 等 |
| `InetAddress.h` / `InetAddress.cpp` | IPv4 地址封装：ip + 端口，网络字节序 |
| `ThreadPool.h` / `ThreadPool.cpp` | 通用线程池：固定数量工作线程，任务队列 + 条件变量 |
| `public.h` | 公共头文件集合 |

### 客户端

| 文件 | 说明 |
|------|------|
| `client.h` / `client.cpp` | 测试客户端：连接服务端，发送 1000 条回显消息，统计条数与耗时；发送完成后保持连接约 5 秒以配合服务端空闲超时关闭 |

## 架构概览

- **主 Reactor**：主线程运行一个 EventLoop，仅负责 Acceptor（listen fd）的 accept；新连接 fd 通过 `runInLoop` 投递到某个 worker。
- **从 Reactor**：多个 worker 线程（默认 2 个），每个运行一个 EventLoop，负责已分配连接的读写；连接使用 **EPOLLET**，在 `handleRead` / `handleWrite` 中读/写至 EAGAIN，避免空转。
- **空闲关闭**：每个 worker EventLoop 使用 timerfd 每 5 秒检查本 loop 上的 Connection，超时未活动则 `forceClose()`（通过 `queueInLoop` 延后执行，避免在 timer 遍历中修改 map）。

## 构建与运行

### 环境

- 支持 C++11 的 g++（Makefile 使用 `-std=c++11 -O2 -Wall -Wextra -pedantic`）

### 编译

```bash
make          # 编译 epoll_server 与 epoll_client
make server   # 仅编译 epoll_server
make client   # 仅编译 epoll_client
make clean    # 清理构建产物
```

### 运行服务端

```bash
./epoll_server        # 默认端口 5005
./epoll_server 5005   # 指定端口
```

### 运行客户端

```bash
./epoll_client <服务端IP> <端口>
# 示例
./epoll_client 127.0.0.1 5005
```

客户端会发送 1000 条消息并等待回显，结束后输出总条数与耗时，然后保持连接直至服务端因 5 秒空闲关闭连接。

## 已实现要点

- epoll（水平触发用于 listen/wakeup/timer，**边缘触发用于连接 fd**）
- Reactor：EventLoop + Channel + Acceptor + Connection
- 主从 Reactor + 线程池
- Buffer 管理每条连接的收发缓冲
- 5 秒空闲超时关闭（每 worker 独立 timerfd）
- 跨线程投递：`runInLoop` / `queueInLoop` + eventfd
- C++11，智能指针管理 EventLoop / Channel / Connection 生命周期
