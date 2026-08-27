#include "elphin/reactor.hpp"
#include <unistd.h>
#include <iostream>

namespace elphin::net {

Reactor::Reactor(int max_events) : events_buffer_(max_events) {
    epoll_fd_ = ::epoll_create1(0);
    if (epoll_fd_ < 0) {
        std::cerr << "[Elphin Engine] Failed to create epoll fd\n";
    }
}

Reactor::~Reactor() {
    if (epoll_fd_ >= 0) {
        ::close(epoll_fd_);
    }
}

bool Reactor::add_fd(int fd, uint32_t events, EventCallback callback) {
    epoll_event ev{};
    ev.events = events;
    ev.data.fd = fd;

    if (::epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev) < 0) {
        return false;
    }
    callbacks_[fd] = std::move(callback);
    return true;
}

bool Reactor::modify_fd(int fd, uint32_t events) {
    epoll_event ev{};
    ev.events = events;
    ev.data.fd = fd;
    return ::epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &ev) >= 0;
}

bool Reactor::remove_fd(int fd) {
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
        if (it != callbacks_.end()) {
            it->second(revents);
        }
    }
}

void Reactor::stop() {
    running_ = false;
}

} // namespace elphin::net