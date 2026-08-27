#include "elphin/connection.hpp"
#include <unistd.h>
#include <cerrno>
#include <iostream>

namespace elphin::net {

Connection::Connection(Reactor* reactor, int fd)
    : reactor_(reactor), fd_(fd) {}

Connection::~Connection() {
    if (fd_ >= 0) {
        ::close(fd_);
    }
}

void Connection::establish_connection() {
    auto self = shared_from_this();
    reactor_->add_fd(fd_, EPOLLIN, [self](uint32_t events) {
        if (events & EPOLLIN) {
            self->handle_read();
        }
        if (events & EPOLLOUT) {
            self->handle_write();
        }
    });
}

void Connection::handle_read() {
    int saved_errno = 0;
    ssize_t n = read_buffer_.read_fd(fd_, &saved_errno);
    if (n > 0) {
        if (message_callback_) {
            message_callback_(shared_from_this(), &read_buffer_);
        }
    } else if (n == 0 || (n < 0 && saved_errno != EAGAIN)) {
        handle_close();
    }
}

void Connection::send(std::string_view msg) {
    write_buffer_.append(msg);
    // Attempt to write directly to the kernel socket buffer
    ssize_t n = ::write(fd_, write_buffer_.peek(), write_buffer_.readable_bytes());
    if (n > 0) {
        write_buffer_.retrieve(n);
    }
    // If the data was not completely sent, register EPOLLOUT to wait for socket buffer availability
    if (write_buffer_.readable_bytes() > 0) {
        auto self = shared_from_this();
        reactor_->modify_fd(fd_, EPOLLIN | EPOLLOUT);
    }
}

void Connection::handle_write() {
    if (write_buffer_.readable_bytes() > 0) {
        ssize_t n = ::write(fd_, write_buffer_.peek(), write_buffer_.readable_bytes());
        if (n > 0) {
            write_buffer_.retrieve(n);
        }
    }
    // Once the write buffer is completely flushed, unregister EPOLLOUT and retain EPOLLIN only
    if (write_buffer_.readable_bytes() == 0) {
        reactor_->modify_fd(fd_, EPOLLIN);
    }
}

void Connection::handle_close() {
    reactor_->remove_fd(fd_);
    if (close_callback_) {
        close_callback_(shared_from_this());
    }
}

} // namespace elphin::net