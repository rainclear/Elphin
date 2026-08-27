#ifndef ELPHIN_BUFFER_HPP
#define ELPHIN_BUFFER_HPP

#include <vector>
#include <string>
#include <string_view>
#include <algorithm>
#include <unistd.h>

namespace elphin::net {

class Buffer {
public:
    explicit Buffer(size_t initial_size = 1024)
        : buffer_(initial_size), read_index_(0), write_index_(0) {}

    size_t readable_bytes() const { return write_index_ - read_index_; }
    size_t writable_bytes() const { return buffer_.size() - write_index_; }

    const char* peek() const { return buffer_.data() + read_index_; }

    void retrieve(size_t len) {
        if (len < readable_bytes()) {
            read_index_ += len;
        } else {
            retrieve_all();
        }
    }

    void retrieve_all() {
        read_index_ = 0;
        write_index_ = 0;
    }

    std::string retrieve_all_to_string() {
        std::string str(peek(), readable_bytes());
        retrieve_all();
        return str;
    }

    void append(const char* data, size_t len) {
        ensure_writable_bytes(len);
        std::copy(data, data + len, buffer_.data() + write_index_);
        write_index_ += len;
    }

    void append(std::string_view str) {
        append(str.data(), str.size());
    }

    // Efficiently reads data directly from a non-blocking Socket descriptor
    ssize_t read_fd(int fd, int* saved_errno);

private:
    void ensure_writable_bytes(size_t len) {
        if (writable_bytes() < len) {
            make_space(len);
        }
    }

    void make_space(size_t len) {
        if (writable_bytes() + read_index_ < len) {
            buffer_.resize(write_index_ + len);
        } else {
            size_t readable = readable_bytes();
            std::copy(buffer_.data() + read_index_, buffer_.data() + write_index_, buffer_.data());
            read_index_ = 0;
            write_index_ = readable;
        }
    }

    std::vector<char> buffer_;
    size_t read_index_;
    size_t write_index_;
};

} // namespace elphin::net

#endif // ELPHIN_BUFFER_HPP