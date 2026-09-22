#include "net/reactor.hpp"
#include "common/logger.hpp"
#include <unistd.h>

namespace elphin::net {

Reactor::Reactor(int max_events) : events_buffer_(max_events) {
    epoll_fd_ = ::epoll_create1(EPOLL_CLOEXEC);
    if (epoll_fd_ < 0) {
        LOG_ERROR("Failed to create epoll_fd");
    }
}

Reactor::~Reactor() {
    if (epoll_fd_ >= 0) {
        ::close(epoll_fd_);
    }
}

bool Reactor::add_fd(int fd, uint32_t events, EventCallback callback) {
    if (fd < 0) return false;

    epoll_event ev{};
    ev.events = events;
    ev.data.fd = fd;

    if (::epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev) < 0) {
        LOG_ERROR("epoll_ctl ADD failed for fd={}", fd);
        return false;
    }

    callbacks_[fd] = std::move(callback);
    return true;
}

bool Reactor::modify_fd(int fd, uint32_t events) {
    if (fd < 0) return false;

    epoll_event ev{};
    ev.events = events;
    ev.data.fd = fd;

    if (::epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &ev) < 0) {
        LOG_ERROR("epoll_ctl MOD failed for fd={}", fd);
        return false;
    }
    return true;
}

bool Reactor::remove_fd(int fd) {
    if (fd < 0) return false;

    callbacks_.erase(fd);
    return ::epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr) >= 0;
}

void Reactor::loop_once(int timeout_ms) {
    int nfds = ::epoll_wait(epoll_fd_, events_buffer_.data(), 
                           static_cast<int>(events_buffer_.size()), timeout_ms);

    for (int i = 0; i < nfds; ++i) {
        int fd = events_buffer_[i].data.fd;
        uint32_t revents = events_buffer_[i].events;

        auto it = callbacks_.find(fd);
        if (it != callbacks_.end() && it->second) {
            // 将真正的 revents（包含 EPOLLIN, EPOLLOUT, EPOLLERR, EPOLLHUP）传给回调
            it->second(revents);
        }
    }
}

void Reactor::run() {
    running_ = true;
    while (running_) {
        loop_once(100);
    }
}

void Reactor::stop() {
    running_ = false;
}

} // namespace elphin::net