#include "InetAddress.h"

InetAddress::InetAddress() {
  std::memset(&addr_, 0, sizeof(addr_));
  addr_.sin_family = AF_INET;
}

InetAddress::InetAddress(const std::string& ip, uint16_t port) {
  std::memset(&addr_, 0, sizeof(addr_));
  addr_.sin_family = AF_INET;
  addr_.sin_port = htons(port);
  if (::inet_pton(AF_INET, ip.c_str(), &addr_.sin_addr) != 1) {
    addr_.sin_addr.s_addr = htonl(INADDR_ANY);
  }
}

InetAddress::InetAddress(uint16_t port) {
  std::memset(&addr_, 0, sizeof(addr_));
  addr_.sin_family = AF_INET;
  addr_.sin_addr.s_addr = htonl(INADDR_ANY);
  addr_.sin_port = htons(port);
}

InetAddress::InetAddress(const sockaddr_in& addr) { addr_ = addr; }

std::string InetAddress::ip() const {
  char buf[INET_ADDRSTRLEN] = {0};
  const char* p = ::inet_ntop(AF_INET, &addr_.sin_addr, buf, sizeof(buf));
  if (!p) return "";
  return std::string(p);
}

uint16_t InetAddress::port() const { return ntohs(addr_.sin_port); }
