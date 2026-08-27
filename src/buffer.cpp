#include "elphin/buffer.hpp"
#include <sys/uio.h>
#include <cerrno>

namespace elphin::net {

ssize_t Buffer::read_fd(int fd, int* saved_errno) {
    // Prepare a 64KB stack buffer to combine with readv (scatter-read), preventing frequent reallocations
    char extra_buf[65536];
    struct iovec vec[2];
    const size_t writable = writable_bytes();

    vec[0].iov_base = buffer_.data() + write_index_;
    vec[0].iov_len = writable;
    vec[1].iov_base = extra_buf;
    vec[1].iov_len = sizeof(extra_buf);

    const int iovcnt = (writable < sizeof(extra_buf)) ? 2 : 1;
    const ssize_t n = ::readv(fd, vec, iovcnt);

    if (n < 0) {
        *saved_errno = errno;
    } else if (static_cast<size_t>(n) <= writable) {
        write_index_ += n;
    } else {
        write_index_ = buffer_.size();
        append(extra_buf, n - writable);
    }
    return n;
}

} // namespace elphin::net