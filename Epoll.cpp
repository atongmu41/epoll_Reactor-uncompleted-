#include "Epoll.h"
#include "Channel.h"

Epoll::Epoll() : epfd_(-1) {}

Epoll::~Epoll() { close(); }

bool Epoll::create() {
  if (epfd_ >= 0) return true;
  epfd_ = ::epoll_create1(EPOLL_CLOEXEC);
  return epfd_ >= 0;
}

void Epoll::close() {
  if (epfd_ >= 0) {
    ::close(epfd_);
    epfd_ = -1;
  }
}

bool Epoll::add(int fd, uint32_t events) {
  if (epfd_ < 0) return false;
  epoll_event ev{};
  ev.events = events;
  ev.data.fd = fd;
  return ::epoll_ctl(epfd_, EPOLL_CTL_ADD, fd, &ev) == 0;
}

bool Epoll::mod(int fd, uint32_t events) {
  if (epfd_ < 0) return false;
  epoll_event ev{};
  ev.events = events;
  ev.data.fd = fd;
  return ::epoll_ctl(epfd_, EPOLL_CTL_MOD, fd, &ev) == 0;
}

bool Epoll::del(int fd) {
  if (epfd_ < 0) return false;
  return ::epoll_ctl(epfd_, EPOLL_CTL_DEL, fd, nullptr) == 0;
}

bool Epoll::add(Channel* ch) {
  if (!ch) return false;
  if (epfd_ < 0) return false;
  epoll_event ev{};
  ev.events = ch->events();
  ev.data.ptr = ch;
  return ::epoll_ctl(epfd_, EPOLL_CTL_ADD, ch->fd(), &ev) == 0;
}

bool Epoll::mod(Channel* ch) {
  if (!ch) return false;
  if (epfd_ < 0) return false;
  epoll_event ev{};
  ev.events = ch->events();
  ev.data.ptr = ch;
  return ::epoll_ctl(epfd_, EPOLL_CTL_MOD, ch->fd(), &ev) == 0;
}

bool Epoll::del(Channel* ch) {
  if (!ch) return false;
  if (epfd_ < 0) return false;
  return ::epoll_ctl(epfd_, EPOLL_CTL_DEL, ch->fd(), nullptr) == 0;
}

int Epoll::wait(std::vector<epoll_event>& events, int timeout_ms) {
  if (epfd_ < 0 || events.empty()) return -1;
  return ::epoll_wait(epfd_, events.data(), static_cast<int>(events.size()), timeout_ms);
}

