#ifndef ELPHIN_NET_ACCEPTOR_HPP
#define ELPHIN_NET_ACCEPTOR_HPP

#include <functional>
#include "net/reactor.hpp"
#include "net/socket_utils.hpp"

namespace elphin::net {

class Acceptor {
public:
    using NewConnectionCallback = std::function<void(int sockfd)>;

    Acceptor(Reactor* reactor, uint16_t port)
        : reactor_(reactor), listen_fd_(create_server_socket(port)) {}

    ~Acceptor() {
        if (listen_fd_ >= 0) {
            reactor_->remove_fd(listen_fd_);
            ::close(listen_fd_);
        }
    }

    bool is_listening() const { return listen_fd_ >= 0; }

    void set_new_connection_callback(NewConnectionCallback cb) {
        new_connection_callback_ = std::move(cb);
    }

    void listen() {
        if (listen_fd_ < 0) return;
        reactor_->add_fd(listen_fd_, EPOLLIN, [this](uint32_t) {
            handle_accept();
        });
    }

private:
    void handle_accept() {
        while (true) {
            sockaddr_in client_addr{};
            socklen_t addr_len = sizeof(client_addr);
            int client_fd = ::accept(listen_fd_, reinterpret_cast<sockaddr*>(&client_addr), &addr_len);

            if (client_fd < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                if (errno == EINTR) continue;
                break;
            }

            set_non_blocking(client_fd);
            set_tcp_nodelay(client_fd);

            if (new_connection_callback_) {
                new_connection_callback_(client_fd);
            } else {
                ::close(client_fd);
            }
        }
    }

    Reactor* reactor_;
    int listen_fd_{-1};
    NewConnectionCallback new_connection_callback_;
};

} // namespace elphin::net

#endif // ELPHIN_NET_ACCEPTOR_HPP