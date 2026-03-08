#include "client.h"

#include "public.h"
#include "InetAddress.h"

int run_client(const ClientOptions& opt) {
  if (opt.port == 0) {
    std::cerr << "invalid port\n";
    return -1;
  }

  ::signal(SIGPIPE, SIG_IGN);

  InetAddress server_addr(opt.host, opt.port);

  int fd = ::socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    std::cerr << "socket failed: " << std::strerror(errno) << "\n";
    return -1;
  }

  if (::connect(fd, reinterpret_cast<sockaddr*>(server_addr.getSockAddrPtr()),
                sizeof(sockaddr_in)) < 0) {
    std::cerr << "connect failed: " << std::strerror(errno) << "\n";
    ::close(fd);
    return -1;
  }

  const int total_msgs = 1000;
  const int batch_size = 10;  // 若恢复 sleep，则每 batch_size 个包 sleep 一次
  (void)batch_size;

  for (int i = 1; i <= total_msgs; ++i) {
    std::string msg = "这是第" + std::to_string(i) + "次测试";

    // 发送完整数据
    size_t sent = 0;
    while (sent < msg.size()) {
      ssize_t n = ::send(fd, msg.data() + sent, msg.size() - sent, 0);
      if (n > 0) {
        sent += static_cast<size_t>(n);
        continue;
      }
      if (n < 0 && errno == EINTR) continue;
      if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) continue;
      std::cerr << "send error: " << std::strerror(errno) << "\n";
      ::close(fd);
      return -1;
    }

    // 接收回显
    std::string recv_buf;
    recv_buf.reserve(msg.size());
    char buf[1024];
    while (recv_buf.size() < msg.size()) {
      ssize_t n = ::recv(fd, buf, sizeof(buf), 0);
      if (n > 0) {
        recv_buf.append(buf, buf + n);
        continue;
      }
      if (n == 0) {
        std::cerr << "server closed connection\n";
        ::close(fd);
        return -1;
      }
      if (n < 0 && errno == EINTR) continue;
      if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) continue;
      std::cerr << "recv error: " << std::strerror(errno) << "\n";
      ::close(fd);
      return -1;
    }

    std::cout << "send: " << msg << "\n";
    std::cout << "recv: " << recv_buf << "\n";

    // 每发送 batch_size 个包后原本 sleep(0.2s)，现按要求注释掉
    // if (i % batch_size == 0) {
    //   ::usleep(200000);  // 0.2 秒
    // }
  }

  ::close(fd);
  return 0;
}

int main(int argc, char** argv) {
  ClientOptions opt;
  if (argc != 3) {
    std::cerr << "usage: " << argv[0] << " <host_ip> <port>\n";
    std::cerr << "example: " << argv[0] << " 192.168.189.129 5005\n";
    return -1;
  }

  opt.host = argv[1];
  opt.port = static_cast<uint16_t>(std::atoi(argv[2]));
  return run_client(opt);
}
