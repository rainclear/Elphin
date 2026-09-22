#include "net/connection.hpp"
#include "common/logger.hpp"
#include <unistd.h>
#include <cerrno>

namespace elphin::net {

Connection::Connection(Reactor* reactor, int fd)
    : reactor_(reactor), fd_(fd) {}

Connection::~Connection() {
    if (fd_ >= 0) {
        ::close(fd_);
    }
}

bool Connection::establish_connection() {
    auto self = shared_from_this();
    return reactor_->add_fd(fd_, EPOLLIN | EPOLLHUP | EPOLLERR, [self](uint32_t events) {
        if (events & (EPOLLERR | EPOLLHUP)) {
            self->handle_close();
            return;
        }
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
    } else if (n == 0 || (n < 0 && saved_errno != EAGAIN && saved_errno != EWOULDBLOCK)) {
        handle_close();
    }
}

void Connection::send(std::string_view msg) {
    if (fd_ < 0) return;

    ssize_t nwrote = 0;
    size_t remaining = msg.size();
    bool fault_error = false;

    if (write_buffer_.readable_bytes() == 0) {
        nwrote = ::write(fd_, msg.data(), msg.size());
        if (nwrote >= 0) {
            remaining = msg.size() - nwrote;
            if (remaining == 0) {
                return;
            }
        } else {
            nwrote = 0;
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                LOG_ERROR("Connection::send write error, errno={}", errno);
                if (errno == EPIPE || errno == ECONNRESET) {
                    fault_error = true;
                }
            }
        }
    }

    if (fault_error) {
        handle_close();
        return;
    }

    if (remaining > 0) {
        write_buffer_.append(msg.data() + nwrote, remaining);
        reactor_->modify_fd(fd_, EPOLLIN | EPOLLOUT | EPOLLHUP | EPOLLERR);
    }
}

void Connection::handle_write() {
    if (write_buffer_.readable_bytes() > 0) {
        ssize_t n = ::write(fd_, write_buffer_.peek(), write_buffer_.readable_bytes());
        if (n > 0) {
            write_buffer_.retrieve(n);
            if (write_buffer_.readable_bytes() == 0) {
                reactor_->modify_fd(fd_, EPOLLIN | EPOLLHUP | EPOLLERR);
            }
        } else {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                LOG_ERROR("Connection::handle_write error, errno={}", errno);
                handle_close();
            }
        }
    }
}

void Connection::handle_close() {
    if (fd_ < 0) return;
    
    int old_fd = fd_;
    reactor_->remove_fd(fd_);
    fd_ = -1; // Mark as closed to avoid double-close

    LOG_INFO("Connection closed on fd={}", old_fd);

    if (close_callback_) {
        close_callback_(shared_from_this());
    }
}

} // namespace elphin::net