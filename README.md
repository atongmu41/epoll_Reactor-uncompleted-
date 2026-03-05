项目名称：基于 epoll + Reactor 的高并发 TCP 服务器（C++11）

一、项目结构概览
1. tcpepoll.h / tcpepoll.cpp
   - 类 EpollServer：主服务端类，基于 Reactor 思想。
   - 职责：
     * 创建监听 Socket（封装在 Socket 类中）、创建 Epoll 实例。
     * 为监听 fd 创建一个监听 Channel（listen_channel_），并注册到 Epoll。
     * 为每个已建立连接创建一个 Channel（channels_），注册读/写/错误/关闭回调。
     * 使用 Epoll::wait 等待事件，拿到 epoll_event.data.ptr 中的 Channel*，调用 Channel::handleEvent 分发事件。
     * 使用 outbuf_ 管理每个连接的写缓冲，实现非阻塞短写与回显逻辑。

2. Channel.h / Channel.cpp
   - 类 Channel：Reactor 中的“事件分发单元”，与一个 fd 绑定。
   - 包含成员：
     * int fd_：底层文件描述符。
     * uint32_t events_：当前关注的 epoll 事件掩码。
     * 读/写/错误/关闭四类回调（std::function<void()>）。
   - handleEvent(revents) 根据 EPOLLIN/EPOLLOUT/EPOLLERR/EPOLLHUP/EPOLLRDHUP 调用对应回调。
   - 注意：发生错误/关闭时优先调用 closeCallback_，内部可能 delete 当前 Channel，随后立即 return，避免 use-after-free。

3. Epoll.h / Epoll.cpp
   - 类 Epoll：对 epoll_create / epoll_ctl / epoll_wait 的轻量封装。
   - 接口：
     * create()/close()/valid()/fd()
     * 对 fd 的 add(fd, events)/mod(fd, events)/del(fd) 封装。
     * 对 Channel* 的 add(Channel*)/mod(Channel*)/del(Channel*) 封装，使用 epoll_event.data.ptr 指向 Channel。
     * wait(std::vector<epoll_event>& events, int timeout_ms) 封装 epoll_wait。

4. Socket.h / Socket.cpp
   - 类 Socket：对 TCP socket 的简单封装。
   - 职责：
     * create() 创建 AF_INET/SOCK_STREAM 套接字。
     * setReuseAddr(bool) 设置 SO_REUSEADDR。
     * setNonBlock() 设置非阻塞。
     * bind(const InetAddress&) 绑定地址。
     * listen(int backlog) 监听。
     * close() 关闭 fd。

5. InetAddress.h / InetAddress.cpp
   - 类 InetAddress：对 IPv4 地址的封装。
   - 职责：
     * 从 ip 字符串 + 端口 或仅端口（0.0.0.0:port）构造 sockaddr_in（网络字节序）。
     * 提供 getSockAddr()/getSockAddrPtr() 用于 bind/connect。
     * 提供 ip()/port() 获取主机字节序的 IP 字符串与端口。

6. client.h / client.cpp
   - 一个简单的测试客户端 epoll_client（当前逻辑已简化为同步收发）。
   - 运行方式：./epoll_client <host_ip> <port>
   - 行为：
     * 连接指定服务端。
     * 连续发送 1000 条文本消息：“这是第 i 次测试”，每次等待对应回显并在终端打印 send/recv 内容。

7. public.h / public.cpp
   - public.h 集中包含项目常用的系统头文件与 C++ 标准库头文件，便于统一使用。

二、构建与运行
1. 编译环境
   - 要求：支持 C++11 的 g++（Makefile 中已指定 -std=c++11）。

2. 使用 Makefile 编译
   - 在项目根目录 /home/atongmu/project/netserver 下执行：
     * make          # 编译服务端 epoll_server 和客户端 epoll_client
     * make server   # 只编译 epoll_server
     * make client   # 只编译 epoll_client
     * make clean    # 清理构建产物

3. 运行服务端
   - 启动服务器（默认端口 5005）：
     * ./epoll_server
   - 或指定端口：
     * ./epoll_server 5005
   - 功能：
     * 非阻塞 epoll + Reactor 模型。
     * 支持多连接的回显服务：收到的数据累积到 outbuf_，在 EPOLLOUT 事件下写回客户端。
     * 在终端打印每次接收与发送的内容，方便调试与观察粘包。

4. 运行客户端
   - 启动测试客户端（示例）：
     * ./epoll_client 127.0.0.1 5005
   - 客户端会连接服务端并自动发送多条“这是第 i 次测试”消息，打印 send/recv 内容，可用于验证服务端逻辑与观察 TCP 粘包现象。

三、当前实现特点与后续扩展方向
1. 已实现内容
   - 基于 epoll 的回显服务器，采用 Reactor 思想初步拆分：
     * InetAddress：地址处理（IP/端口、字节序转换）。
     * Socket：套接字创建与基础选项封装。
     * Epoll：epoll 系统调用封装，支持 fd 与 Channel* 两种接口。
     * Channel：对单个 fd 的事件与回调封装，epoll_event.data.ptr 指向 Channel。
     * EpollServer：组合以上组件，实现完整的事件循环与连接管理。
   - 简单客户端用于功能与性能测试。
   - 完全基于 C++11 编译通过（-std=c++11 -Wall -Wextra -pedantic）。

2. 可进一步扩展的方向
   - 增加 EventLoop / EventLoopThread / EventLoopThreadPool，支持多 Reactor 线程。
   - 引入 TcpConnection 抽象，将读写缓冲、业务回调从 EpollServer 中进一步拆分。
   - 增加简单应用层协议（如长度前缀、换行分隔）并在服务端实现半包/粘包的拆包逻辑。
   - 增加日志模块、定时器、连接超时管理等。

