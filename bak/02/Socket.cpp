#include "Socket.h"

Socket::Socket() : fd_(-1) {}

Socket::Socket(int fd) : fd_(fd) {}

Socket::~Socket() { close(); }

bool Socket::create() {
  if (fd_ >= 0) return true;
  fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
  return fd_ >= 0;
}

bool Socket::setReuseAddr(bool on) {
  if (fd_ < 0) return false;
  int opt = on ? 1 : 0;
  return ::setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == 0;
}

bool Socket::setNonBlock() {
  if (fd_ < 0) return false;
  int flags = ::fcntl(fd_, F_GETFL, 0);
  if (flags == -1) return false;
  return ::fcntl(fd_, F_SETFL, flags | O_NONBLOCK) == 0;
}

bool Socket::bind(const InetAddress& addr) {
  if (fd_ < 0) return false;
  const sockaddr_in& a = addr.getSockAddr();
  return ::bind(fd_, reinterpret_cast<const sockaddr*>(&a), sizeof(a)) == 0;
}

bool Socket::listen(int backlog) const {
  if (fd_ < 0) return false;
  return ::listen(fd_, backlog) == 0;
}

void Socket::close() {
  if (fd_ >= 0) {
    ::close(fd_);
    fd_ = -1;
  }
}

