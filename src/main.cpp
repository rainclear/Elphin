#include <iostream>
#include <unistd.h>
#include "elphin/common.hpp"
#include "elphin/socket_utils.hpp"
#include "elphin/reactor.hpp"

int main() {
    std::cout << "Starting " << elphin::PROJECT_NAME << " v" << elphin::VERSION << "...\n";

    constexpr uint16_t PORT = 6379;
    int listen_fd = elphin::net::create_server_socket(PORT);
    if (listen_fd < 0) {
        std::cerr << "Failed to bind listener on port " << PORT << "\n";
        return 1;
    }

    std::cout << "[Elphin Core] Listening on port " << PORT << "...\n";

    elphin::net::Reactor reactor;

    // Handle incoming connections on listener socket
    reactor.add_fd(listen_fd, EPOLLIN, [&](uint32_t events) {
        if (events & EPOLLIN) {
            sockaddr_in client_addr{};
            socklen_t addr_len = sizeof(client_addr);
            int client_fd = ::accept(listen_fd, reinterpret_cast<sockaddr*>(&client_addr), &addr_len);

            if (client_fd >= 0) {
                elphin::net::set_non_blocking(client_fd);
                elphin::net::set_tcp_nodelay(client_fd);
                std::cout << "[Elphin] Accepted connection on fd: " << client_fd << "\n";

                // Echo back for quick verification
                reactor.add_fd(client_fd, EPOLLIN, [&reactor, client_fd](uint32_t client_events) {
                    if (client_events & EPOLLIN) {
                        char buf[256];
                        ssize_t bytes_read = ::read(client_fd, buf, sizeof(buf) - 1);
                        if (bytes_read > 0) {
                            buf[bytes_read] = '\0';
                            std::cout << "[Client " << client_fd << "]: " << buf;
                            ::write(client_fd, "+PONG\r\n", 7);
                        } else {
                            std::cout << "[Elphin] Client disconnected fd: " << client_fd << "\n";
                            reactor.remove_fd(client_fd);
                            ::close(client_fd);
                        }
                    }
                });
            }
        }
    });

    while (true) {
        reactor.loop_once(100);
    }

    ::close(listen_fd);
    return 0;
}