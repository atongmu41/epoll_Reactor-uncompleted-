#include "Channel.h"

Channel::Channel(int fd) : fd_(fd) {}

void Channel::handleEvent(uint32_t revents) {
  // 错误/关闭优先：若有 closeCallback_ 则只调它并返回（内部可能 delete this）；否则仅调 errorCallback_（如 listen 仅打日志）
  if ((revents & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) != 0) {
    if (closeCallback_) {
      closeCallback_();
      return;  // 避免 use-after-free：close_conn 会 delete this，不可再访问成员或调用其他回调
    }
    if (errorCallback_) errorCallback_();
  }

  if ((revents & EPOLLIN) != 0) {
    if (readCallback_) readCallback_();
  }

  if ((revents & EPOLLOUT) != 0) {
    if (writeCallback_) writeCallback_();
  }
}

