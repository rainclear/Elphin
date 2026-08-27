#ifndef ELPHIN_SOCKET_UTILS_HPP
#define ELPHIN_SOCKET_UTILS_HPP

#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>
#include <iostream>

namespace elphin::net {

inline bool set_non_blocking(int fd) {
    int flags = ::fcntl(fd, F_GETFL, 0);
    if (flags == -1) return false;
    return ::fcntl(fd, F_SETFL, flags | O_NONBLOCK) != -1;
}

inline bool set_tcp_nodelay(int fd) {
    int opt = 1;
    return ::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt)) == 0;
}

inline bool set_reuse_addr(int fd) {
    int opt = 1;
    // Added 'fd' as the first parameter
    return ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == 0;
}

inline int create_server_socket(uint16_t port) {
    int listen_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) return -1;

    set_reuse_addr(listen_fd);
    set_non_blocking(listen_fd);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (::bind(listen_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        ::close(listen_fd);
        return -1;
    }

    if (::listen(listen_fd, SOMAXCONN) < 0) {
        ::close(listen_fd);
        return -1;
    }

    return listen_fd;
}

} // namespace elphin::net

#endif // ELPHIN_SOCKET_UTILS_HPP